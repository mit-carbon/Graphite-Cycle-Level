#include <cmath>
#include "packet_buffer_flow_control_scheme.h"
#include "log.h"

PacketBufferFlowControlScheme::PacketBufferFlowControlScheme(
      SInt32 num_input_channels, SInt32 num_output_channels,
      vector<SInt32>& num_input_endpoints_list, vector<SInt32>& num_output_endpoints_list,
      vector<BufferManagementScheme::Type>& input_buffer_management_scheme_vec,
      vector<BufferManagementScheme::Type>& downstream_buffer_management_scheme_vec,
      vector<SInt32>& input_buffer_size_vec, vector<SInt32>& downstream_buffer_size_vec):
   FlowControlScheme(num_input_channels, num_output_channels)
{
   // Create Input Queues
   _input_packet_buffer_vec.resize(_num_input_channels);
   for (SInt32 i = 0; i < _num_input_channels; i++)
   {
      _input_packet_buffer_vec[i] = PacketBuffer::create(input_buffer_management_scheme_vec[i],
                                                         input_buffer_size_vec[i]);
   }

   // Create downstream buffer usage histories
   _vec_downstream_buffer_status_list.resize(_num_output_channels);
   for (SInt32 i = 0; i < _num_output_channels; i++)
   {
      _vec_downstream_buffer_status_list[i] =  new BufferStatusList(num_output_endpoints_list[i],
            downstream_buffer_management_scheme_vec[i], downstream_buffer_size_vec[i]);
   }
}

PacketBufferFlowControlScheme::~PacketBufferFlowControlScheme()
{   
   for (SInt32 i = 0; i < _num_input_channels; i++)
   {
      delete _input_packet_buffer_vec[i];
   }
   for (SInt32 i = 0; i < _num_output_channels; i++)
   {
      delete _vec_downstream_buffer_status_list[i];
   }
}

void
PacketBufferFlowControlScheme::processDataMsg(Flit* flit, vector<NetworkMsg*>& network_msg_list)
{
   _network_msg_list = &network_msg_list;

   assert(flit->_type == Flit::HEAD);
   Channel::Endpoint& input_endpoint = flit->_input_endpoint;
   
   PacketBuffer* packet_buffer = _input_packet_buffer_vec[input_endpoint._channel_id];
   BufferManagementMsg* upstream_buffer_msg = packet_buffer->enqueue(flit);

   if (upstream_buffer_msg)
   {
      upstream_buffer_msg->_input_endpoint = flit->_input_endpoint;
      _network_msg_list->push_back(upstream_buffer_msg);
   }

   // Iterate through the input buffers and send any ready messages
   iterate();
}

void
PacketBufferFlowControlScheme::processBufferManagementMsg(BufferManagementMsg* buffer_management_msg,
                                                          vector<NetworkMsg*>& network_msg_list)
{
   _network_msg_list = &network_msg_list;

   // Only credit-based buffer management scheme can be used with Store-and-forward flow control
   assert(buffer_management_msg->_type == BufferManagementScheme::CREDIT);
   Channel::Endpoint& output_endpoint = buffer_management_msg->_output_endpoint;
   BufferStatusList* buffer_status_list = _vec_downstream_buffer_status_list[output_endpoint._channel_id];
   buffer_status_list->receiveBufferManagementMsg(buffer_management_msg, output_endpoint._index);
   
   // Iterate through the input buffers and send any ready messages
   // FIXME: Have a monitor here to check the size of the credit history
   iterate();
}

void
PacketBufferFlowControlScheme::iterate()
{
   // Both Store-and-Forward and Virtual-Cut-Through flow control require
   // credit-based backpressure scheme
   // They both have to know total number of free flits in the downstream
   // router in order to do allocate a buffer
   // All packets should be normalized by the same function
   // Not sure how to translate link occupancies into buffer occupancies
  
   bool processing_finished; 
   do
   {
      // Process all the input channels in FIFO order
      processing_finished = true;
      for (SInt32 input_channel = 0; input_channel < _num_input_channels; input_channel++)
      {
         bool sent = sendPacket(input_channel);
         if (sent)
         {
            processing_finished = false;
         }
      }
   } while (! processing_finished);
}

bool
PacketBufferFlowControlScheme::sendPacket(SInt32 input_channel)
{
   PacketBuffer* packet_buffer = _input_packet_buffer_vec[input_channel];
   if (packet_buffer->empty())
   {
      return false;
   }

   HeadFlit* head_flit = (HeadFlit*) packet_buffer->front();
   
   // Update Flit Time
   packet_buffer->updateFlitTime();

   // Allocate Downstream Buffers for all output endpoints

   UInt64 max_allocated_time = 0;

   vector<Channel::Endpoint>::iterator endpoint_it = head_flit->_output_endpoint_list->begin();
   for ( ; endpoint_it != head_flit->_output_endpoint_list->end(); endpoint_it ++)
   {
      Channel::Endpoint output_endpoint = *endpoint_it;

      UInt64 allocated_time = tryAllocateDownstreamBuffer(head_flit, output_endpoint);
      if (allocated_time == UINT64_MAX_)
      {
         LOG_PRINT("Could not allocate buffers for endpoint(%i,%i)",
               output_endpoint._channel_id, output_endpoint._index);
         return false;
      }

      // Compute the maximum
      max_allocated_time = max<UInt64>(max_allocated_time, allocated_time);
   }

   // All output endpoints have a free buffer
   LOG_PRINT("Buffer allocated downstream for all output endpoints: Allocated Time(%llu)", max_allocated_time);

   // Update Flit Time to max_allocated_time
   head_flit->_normalized_time = max<UInt64>(head_flit->_normalized_time, max_allocated_time);
   
   // All downstream buffers have been allocated
   endpoint_it = head_flit->_output_endpoint_list->begin();
   for ( ; endpoint_it != head_flit->_output_endpoint_list->end(); endpoint_it ++)
   {
      Channel::Endpoint output_endpoint = *endpoint_it;

      allocateDownstreamBuffer(head_flit, output_endpoint);

      // Send Packet Downstream
      // Duplicate head_flit and net_packet
      NetPacket* cloned_net_packet = head_flit->_net_packet->clone();
      HeadFlit* cloned_head_flit = (HeadFlit*) cloned_net_packet->data;
      cloned_head_flit->_net_packet = cloned_net_packet;
      cloned_head_flit->_output_endpoint = output_endpoint;
     
      // Send cloned_head_flit downstream
      _network_msg_list->push_back(cloned_head_flit);
   }

   // Remove head flit from queue
   // Update Buffer Time first
   packet_buffer->updateBufferTime();
   
   // Remove head_flit from queue
   BufferManagementMsg* upstream_buffer_msg = packet_buffer->dequeue();
   if (upstream_buffer_msg)
   {
      upstream_buffer_msg->_input_endpoint = head_flit->_input_endpoint;
      _network_msg_list->push_back(upstream_buffer_msg);
   }
         
   // Delete _output_endpoint_list
   delete head_flit->_output_endpoint_list;

   // Release the Net-packet and hence the flit
   head_flit->_net_packet->release();

   return true;
}

void
PacketBufferFlowControlScheme::allocateDownstreamBuffer(HeadFlit* head_flit, Channel::Endpoint& output_endpoint)
{
   BufferStatusList* buffer_status_list = _vec_downstream_buffer_status_list[output_endpoint._channel_id];
   buffer_status_list->allocateBuffer(head_flit, output_endpoint._index, head_flit->_num_phits);
}

UInt64
PacketBufferFlowControlScheme::tryAllocateDownstreamBuffer(HeadFlit* head_flit, Channel::Endpoint& output_endpoint)
{
   BufferStatusList* buffer_status_list = _vec_downstream_buffer_status_list[output_endpoint._channel_id];
   return buffer_status_list->tryAllocateBuffer(head_flit, output_endpoint._index, head_flit->_num_phits);
}

void
PacketBufferFlowControlScheme::dividePacket(NetPacket* net_packet, list<NetPacket*>& net_packet_list,
                                            SInt32 serialization_latency)
{
   LOG_PRINT("PACKET_BUFFER: dividePacket(%p,%i) enter", net_packet, serialization_latency);
   Flit* head_flit = new HeadFlit(1, serialization_latency, net_packet->sender, net_packet->receiver);
   NetPacket* head_flit_packet = new NetPacket(net_packet->time, net_packet->type,
         head_flit->size(), (void*) head_flit,
         false /* is_raw */, net_packet->sequence_num);
   head_flit_packet->start_time = net_packet->start_time;
   net_packet_list.push_back(head_flit_packet);
   LOG_PRINT("PACKET_BUFFER: dividePacket(%p,%i) exit", net_packet, serialization_latency);
}

bool
PacketBufferFlowControlScheme::isPacketComplete(Flit::Type flit_type)
{
   assert(flit_type == Flit::HEAD);
   return true;
}
