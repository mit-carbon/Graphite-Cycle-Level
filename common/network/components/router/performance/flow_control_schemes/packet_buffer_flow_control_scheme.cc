#include <cmath>
#include "packet_buffer_flow_control_scheme.h"

PacketBufferFlowControlScheme::PacketBufferFlowControlScheme( \
      SInt32 num_input_channels, SInt32 num_output_channels, \
      vector<SInt32>& num_input_endpoints_list, vector<SInt32>& num_output_endpoints_list, \
      vector<BufferManagementScheme::Type>& input_buffer_management_scheme_vec, \
      vector<BufferManagementScheme::Type>& downstream_buffer_management_scheme_vec, \
      vector<SInt32>& input_buffer_size_vec, vector<SInt32>& downstream_buffer_size_vec):
   FlowControlScheme(num_input_channels, num_output_channels)
{
   // Create Input Queues
   _input_packet_buffer_vec.resize(_num_input_channels);
   for (SInt32 i = 0; i < _num_input_channels; i++)
   {
      _input_packet_buffer_vec[i] = PacketBuffer::create( \
            input_buffer_management_scheme_vec[i], input_buffer_size_vec[i]);
   }

   // Create downstream buffer usage histories
   _list_of_downstream_buffer_usage_histories_vec.resize(_num_output_channels);
   for (SInt32 i = 0; i < _num_output_channels; i++)
   {
      _list_of_downstream_buffer_usage_histories_vec[i] =  new ListOfBufferUsageHistories( \
            num_output_endpoints_list[i], \
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
      delete _list_of_downstream_buffer_usage_histories_vec[i];
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
PacketBufferFlowControlScheme::processBufferManagementMsg(BufferManagementMsg* buffer_management_msg, \
      vector<NetworkMsg*>& network_msg_list)
{
   _network_msg_list = &network_msg_list;

   // Only credit-based buffer management scheme can be used with Store-and-forward flow control
   assert(buffer_management_msg->_type == BufferManagementScheme::CREDIT);
   Channel::Endpoint& output_endpoint = buffer_management_msg->_output_endpoint;
   ListOfBufferUsageHistories* list_of_buffer_usage_histories = \
         _list_of_downstream_buffer_usage_histories_vec[output_endpoint._channel_id];
   list_of_buffer_usage_histories->receiveBufferManagementMsg( \
         buffer_management_msg, output_endpoint._index);
   
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

   Flit* head_flit = packet_buffer->front();

   head_flit->_output_endpoint = head_flit->_output_endpoint_list->curr();
   // Update Flit Time
   if (head_flit->_output_endpoint == head_flit->_output_endpoint_list->first())
   {
      packet_buffer->updateFlitTime();
   }

   bool allocated = allocateDownstreamBuffer(head_flit);
   if (allocated)
   {
      // head_flit_to_send downstream
      Flit* head_flit_to_send;

      // Remove flit from queue
      // Update Buffer Time first
      packet_buffer->updateBufferTime();
      if (head_flit->_output_endpoint == head_flit->_output_endpoint_list->last())
      {
         // Remove head_flit from queue
         // Update upstream buffer usage history
         BufferManagementMsg* upstream_buffer_msg = packet_buffer->dequeue();
         if (upstream_buffer_msg)
         {
            upstream_buffer_msg->_input_endpoint = head_flit->_input_endpoint;
            _network_msg_list->push_back(upstream_buffer_msg);
         }
         // Send head_flit downstream
         head_flit_to_send = head_flit;
         // Delete _output_endpoint_list
         delete head_flit->_output_endpoint_list;
      }
      else
      {
         // Duplicate head_flit and net_packet
         NetPacket* cloned_net_packet = head_flit->_net_packet->clone();
         Flit* cloned_head_flit = (Flit*) cloned_net_packet->data;
         cloned_head_flit->_net_packet = cloned_net_packet;
         
         // Send cloned_head_flit downstream
         head_flit_to_send = cloned_head_flit;
         // Update pointer to _output_endpoint_list
         head_flit->_output_endpoint_list->incr();
      }

      // Send packet to downstream router
      _network_msg_list->push_back(head_flit_to_send);

      return true;
   }
   else
   {
      return false;
   }
}

bool
PacketBufferFlowControlScheme::allocateDownstreamBuffer(Flit* head_flit)
{
   Channel::Endpoint& output_endpoint = head_flit->_output_endpoint;
   ListOfBufferUsageHistories* list_of_buffer_usage_histories = \
         _list_of_downstream_buffer_usage_histories_vec[output_endpoint._channel_id];
   return list_of_buffer_usage_histories->allocateBuffer(head_flit, output_endpoint._index);
}

void
PacketBufferFlowControlScheme::dividePacket(NetPacket* net_packet, \
      list<NetPacket*>& net_packet_list, \
      SInt32 num_flits, core_id_t requester)
{
   Flit* head_flit = new Flit(Flit::HEAD, num_flits, net_packet->sender, net_packet->receiver, requester);
   NetPacket* head_flit_packet = new NetPacket(net_packet->time, net_packet->type, \
         head_flit->size(), (void*) head_flit, \
         false /* is_raw */, net_packet->sequence_num);
   net_packet_list.push_back(head_flit_packet);
}

bool
PacketBufferFlowControlScheme::isPacketComplete(NetPacket* net_packet)
{
   assert(!net_packet->is_raw);
   NetworkMsg* network_msg = (NetworkMsg*) net_packet->data;
   assert(network_msg->_type == NetworkMsg::DATA);
   Flit* flit = (Flit*) network_msg;
   assert(flit->_type == Flit::HEAD);
   return true;
}
