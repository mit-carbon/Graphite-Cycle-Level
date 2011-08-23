#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include "head_flit.h"
#include "wormhole_flow_control_scheme.h"
#include "log.h"

WormholeFlowControlScheme::WormholeFlowControlScheme(
      SInt32 num_input_channels, SInt32 num_output_channels,
      vector<SInt32>& num_input_endpoints_list, vector<SInt32>& num_output_endpoints_list,
      vector<BufferManagementScheme::Type>& input_buffer_management_scheme_vec,
      vector<BufferManagementScheme::Type>& downstream_buffer_management_scheme_vec,
      vector<SInt32>& input_buffer_size_vec, vector<SInt32>& downstream_buffer_size_vec):
   FlitBufferFlowControlScheme(num_input_channels, num_output_channels)
{
   // Create Input Queues
   _input_flit_buffer_vec.resize(_num_input_channels);
   for (SInt32 i = 0; i < _num_input_channels; i++)
   {
      _input_flit_buffer_vec[i] = new FlitBuffer(input_buffer_management_scheme_vec[i],
            input_buffer_size_vec[i]);
   }

   // Create downstream buffer usage histories
   _vec_downstream_buffer_status_list.resize(_num_output_channels);
   for (SInt32 i = 0; i < _num_output_channels; i++)
   {
      _vec_downstream_buffer_status_list[i] = new BufferStatusList(num_output_endpoints_list[i],
            downstream_buffer_management_scheme_vec[i], downstream_buffer_size_vec[i]);
   }

   // Allocate output channels to input buffers
   _input_channels_allocated_vec.resize(_num_output_channels);
   for (SInt32 i = 0; i < _num_output_channels; i++)
   {
      _input_channels_allocated_vec[i] = Channel::INVALID;
   }
}

WormholeFlowControlScheme::~WormholeFlowControlScheme()
{
   for (SInt32 i = 0; i < _num_input_channels; i++)
   {
      delete _input_flit_buffer_vec[i];
   }
   for (SInt32 i = 0; i < _num_output_channels; i++)
   {
      delete _vec_downstream_buffer_status_list[i];
   }
}

void
WormholeFlowControlScheme::processDataMsg(Flit* flit, vector<NetworkMsg*>& network_msg_list)
{
   LOG_PRINT("processDataMsg(%p, %p) enter", this, flit);

   _network_msg_list = &network_msg_list;

   Channel::Endpoint& input_endpoint = flit->_input_endpoint;
   LOG_PRINT("Input Endpoint(%i, %i)", input_endpoint._channel_id, input_endpoint._index);

   FlitBuffer* flit_buffer = _input_flit_buffer_vec[input_endpoint._channel_id];
   BufferManagementMsg* upstream_buffer_msg = flit_buffer->enqueue(flit);

   if (upstream_buffer_msg)
   {
      upstream_buffer_msg->_input_endpoint = flit->_input_endpoint;
      _network_msg_list->push_back(upstream_buffer_msg);
   }

   iterate();
   
   LOG_PRINT("processDataMsg(%p, %p) exit", this, flit);
}

void
WormholeFlowControlScheme::processBufferManagementMsg(BufferManagementMsg* buffer_management_msg,
                                                      vector<NetworkMsg*>& network_msg_list)
{
   LOG_PRINT("processBufferManagementMsg(%p, %p) enter", this, buffer_management_msg);

   _network_msg_list = &network_msg_list;

   Channel::Endpoint& output_endpoint = buffer_management_msg->_output_endpoint;
   BufferStatusList* buffer_status_list = _vec_downstream_buffer_status_list[output_endpoint._channel_id];
   buffer_status_list->receiveBufferManagementMsg(buffer_management_msg, output_endpoint._index);
 
   iterate();
   
   LOG_PRINT("processBufferManagementMsg(%p, %p) exit", this, buffer_management_msg);
}


void
WormholeFlowControlScheme::iterate()
{
   LOG_PRINT("iterate(%p) enter", this);

   bool processing_finished;
   do
   {
      // Process all the input channels in FIFO order (starting at the lowest channel first)
      processing_finished = true;
      for (SInt32 input_channel = 0; input_channel < _num_input_channels; input_channel++)
      {
         bool done = false;
         while (!done)
         {
            pair<bool,bool> sent = sendFlit(input_channel);
            if (sent.first && sent.second) // flit_sent && packet_sent
            {
               done = true;
               processing_finished = false;
            }
            else if (sent.first && !sent.second) // flit_sent && !packet_sent
            {
            }
            else if (!sent.first && !sent.second) // !flit_sent && !packet_sent
            {
               done = true;
            }
            else
            {
               LOG_PRINT_ERROR("sent.first(false) and sent.second(true)");
            }
         } // while (!done)
      } // for (SInt32 input_channel = 0; input_channel < _num_input_channels; input_channel++)
   } while (! processing_finished);
   
   LOG_PRINT("iterate(%p) exit", this);
}

// ret.first : true if sending flit is successful
// ret.second : true if sending packet is successful
pair<bool, bool>
WormholeFlowControlScheme::sendFlit(SInt32 input_channel)
{
   LOG_PRINT("sendFlit(%i) enter", input_channel);

   FlitBuffer* flit_buffer = _input_flit_buffer_vec[input_channel];

   LOG_PRINT("Got flit_buffer(%p)", flit_buffer);

   if (flit_buffer->empty())
   {
      LOG_PRINT("sendFlit(%i) exit->(false,false)", input_channel);
      return make_pair<bool,bool>(false,false);
   }

   Flit* flit = flit_buffer->front();

   LOG_PRINT("Got Flit(%p): Sender(%i), Receiver(%i)", flit, flit->_sender, flit->_receiver);

   if (flit_buffer->_output_endpoint_list == NULL)
   {
      LOG_PRINT("Head Flit");
      LOG_ASSERT_ERROR(flit->_type & Flit::HEAD, "flit->_type(%u)", flit->_type);
      HeadFlit* head_flit = (HeadFlit*) flit;
      flit_buffer->_output_endpoint_list = head_flit->_output_endpoint_list;
   }

   // Update Flit Time First
   LOG_PRINT("Update Flit Time");
   flit_buffer->updateFlitTime();
   
   if (!flit_buffer->_output_channels_allocated)
   {
      LOG_ASSERT_ERROR(flit->_type & Flit::HEAD, "flit->_type(%u)", flit->_type);

      // Iterate through the list of output endpoints and try to allocate channels
      // all at once - otherwise deadlock results
      vector<Channel::Endpoint>::iterator endpoint_it = flit_buffer->_output_endpoint_list->begin();
      for ( ; endpoint_it != flit_buffer->_output_endpoint_list->end(); endpoint_it ++)
      {
         Channel::Endpoint output_endpoint = *endpoint_it;
         SInt32 output_channel = output_endpoint._channel_id;

         // FIXME: This can occur if multiple output endpoints have same output channels
         LOG_PRINT("Output Endpoint(%i,%i)", output_endpoint._channel_id, output_endpoint._index);
         LOG_ASSERT_ERROR(_input_channels_allocated_vec[output_channel] != input_channel,
               "Output Channel(%i) already allocated for input channel(%i)",
               output_channel, input_channel);
         
         if (_input_channels_allocated_vec[output_channel] != Channel::INVALID)
         {
            // Flits of some other flow are occupying the output channel to use
            LOG_PRINT("Output Channel(%i) already allocated to input channel(%i)",
                  output_channel, _input_channels_allocated_vec[output_channel]);
            
            LOG_PRINT("sendFlit(%i) exit->(false,false)", input_channel);
            return make_pair<bool,bool>(false,false);
         }
      }

      // All output channels are not allocated. Allocate all output channels at once
      endpoint_it = flit_buffer->_output_endpoint_list->begin();
      for ( ; endpoint_it != flit_buffer->_output_endpoint_list->end(); endpoint_it ++)
      {
         SInt32 output_channel = (*endpoint_it)._channel_id;
         LOG_PRINT("Allocating Output Channel(%i)", output_channel);
         _input_channels_allocated_vec[output_channel] = input_channel;
      }

      // Completed allocating output channels
      flit_buffer->_output_channels_allocated = true;
   }

   // All Output Channels are now allocated

   // Now, allocate a downstream buffer
   UInt64 max_allocated_time = 0;

   vector<Channel::Endpoint>::iterator endpoint_it = flit_buffer->_output_endpoint_list->begin();
   for ( ; endpoint_it != flit_buffer->_output_endpoint_list->end(); endpoint_it ++)
   {
      Channel::Endpoint output_endpoint = *endpoint_it;
      
      UInt64 allocated_time = tryAllocateDownstreamBuffer(flit, output_endpoint);
      if (allocated_time == UINT64_MAX)
      {
         LOG_PRINT("Could not allocate a buffer for output endpoint(%i,%i)",
               output_endpoint._channel_id, output_endpoint._index);

         LOG_PRINT("sendFlit(%i) exit->(false,false)", input_channel);
         return make_pair<bool,bool>(false,false);
      }

      // Compute the maximum
      max_allocated_time = max<UInt64>(max_allocated_time, allocated_time);
   }

   // All output endpoints have a free buffer
   LOG_PRINT("Buffer allocated downstream for all output endpoints: Allocated Time(%llu)", max_allocated_time);

   // Update Flit Time to max_allocated_time
   flit->_normalized_time = max<UInt64>(flit->_normalized_time, max_allocated_time);
   
   // Send Flit to all output endpoints
   endpoint_it = flit_buffer->_output_endpoint_list->begin();
   for ( ; endpoint_it != flit_buffer->_output_endpoint_list->end(); endpoint_it ++)
   {
      Channel::Endpoint output_endpoint = *endpoint_it;
      LOG_PRINT("Downstream Buffer allocated for Output Endpoint(%i,%i)",
            output_endpoint._channel_id, output_endpoint._index);
      
      allocateDownstreamBuffer(flit, output_endpoint);

      // Duplicate flit and net_packet
      NetPacket* cloned_net_packet = flit->_net_packet->clone();
      Flit* cloned_flit = (Flit*) cloned_net_packet->data;
      cloned_flit->_net_packet = cloned_net_packet;
      cloned_flit->_output_endpoint = output_endpoint;
         
      LOG_PRINT("Adding to list of messages to send");

      // Send flit to downstream router
      _network_msg_list->push_back(cloned_flit);

      // If TAIL Flit, de-allocate output channel
      if (flit->_type & Flit::TAIL)
      {
         SInt32 output_channel = output_endpoint._channel_id;
         
         LOG_PRINT("TAIL Flit: Releasing Output Channel(%i)", output_channel);
         LOG_ASSERT_ERROR(_input_channels_allocated_vec[output_channel] == input_channel,
               "Output Channel(%i) allocated to Input Channel(%i), should be allocated to (%i)",
               output_channel, _input_channels_allocated_vec[output_channel], input_channel);

         _input_channels_allocated_vec[output_channel] = Channel::INVALID;
      }
   }

   // Update Buffer Time for next flit
   LOG_PRINT("Updating Buffer Time");
   flit_buffer->updateBufferTime();

   // Remove flit from queue
   BufferManagementMsg* upstream_buffer_msg = flit_buffer->dequeue();
   if (upstream_buffer_msg)
   {
      LOG_PRINT("Sending Upstream Buffer Msg (%p)", upstream_buffer_msg);

      upstream_buffer_msg->_input_endpoint = flit->_input_endpoint;
      _network_msg_list->push_back(upstream_buffer_msg);
   }

   // Move the flit_type to a local variable
   Flit::Type flit_type = flit->_type;
   
   // Release the Net-packet and hence the flit
   flit->_net_packet->release();

   // If TAIL Flit, de-allocate the output channels
   if (flit_type & Flit::TAIL)
   {
      LOG_PRINT("TAIL flit");

      // All flits of the packet have been sent on all output channels
      // Delete _output_endpoint_list
      delete flit_buffer->_output_endpoint_list;
      flit_buffer->_output_endpoint_list = NULL;

      // Set Output Channels allocated to false
      flit_buffer->_output_channels_allocated = false;
   
      LOG_PRINT("sendFlit(%i) exit->(true,true)", input_channel);
      return make_pair<bool,bool>(true,true);
   }
   else
   {
      // There is at least one flit remaining to be sent on all output channels
      LOG_PRINT("sendFlit(%i) exit->(true,false)", input_channel);
      return make_pair<bool,bool>(true,false);
   }
}

void
WormholeFlowControlScheme::allocateDownstreamBuffer(Flit* flit, Channel::Endpoint& output_endpoint)
{
   LOG_ASSERT_ERROR(flit->_num_phits == 1, "Num Phits(%i)", flit->_num_phits);
   BufferStatusList* buffer_status_list = _vec_downstream_buffer_status_list[output_endpoint._channel_id];
   buffer_status_list->allocateBuffer(flit, output_endpoint._index, flit->_num_phits);
}

UInt64
WormholeFlowControlScheme::tryAllocateDownstreamBuffer(Flit* flit, Channel::Endpoint& output_endpoint)
{
   LOG_ASSERT_ERROR(flit->_num_phits == 1, "Num Phits(%i), Flit(%s)", flit->_num_phits, flit->getTypeString().c_str());
   BufferStatusList* buffer_status_list = _vec_downstream_buffer_status_list[output_endpoint._channel_id];
   return buffer_status_list->tryAllocateBuffer(flit, output_endpoint._index, flit->_num_phits);
}
