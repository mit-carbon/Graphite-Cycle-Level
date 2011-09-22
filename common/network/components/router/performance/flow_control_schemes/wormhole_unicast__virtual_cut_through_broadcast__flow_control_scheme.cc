#include "head_flit.h"
#include "wormhole_unicast__virtual_cut_through_broadcast__flow_control_scheme.h"
#include "log.h"

WormholeUnicastVirtualCutThroughBroadcastFlowControlScheme::WormholeUnicastVirtualCutThroughBroadcastFlowControlScheme(
      SInt32 num_input_channels, SInt32 num_output_channels,
      vector<SInt32>& num_input_endpoints_list, vector<SInt32>& num_output_endpoints_list,
      vector<BufferManagementScheme::Type>& input_buffer_management_scheme_vec,
      vector<BufferManagementScheme::Type>& downstream_buffer_management_scheme_vec,
      vector<SInt32>& input_buffer_size_vec,
      vector<SInt32>& downstream_buffer_size_vec)
   : WormholeFlowControlScheme(num_input_channels, num_output_channels,
                               num_input_endpoints_list, num_output_endpoints_list,
                               input_buffer_management_scheme_vec, downstream_buffer_management_scheme_vec,
                               input_buffer_size_vec, downstream_buffer_size_vec)
{}

WormholeUnicastVirtualCutThroughBroadcastFlowControlScheme::~WormholeUnicastVirtualCutThroughBroadcastFlowControlScheme()
{}

// ret.first : true if sending flit is successful
// ret.second : true if sending packet is successful
pair<bool, bool>
WormholeUnicastVirtualCutThroughBroadcastFlowControlScheme::sendFlit(SInt32 input_channel)
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
  
   if (flit->_receiver == NetPacket::BROADCAST) 
   {
      LOG_ASSERT_ERROR(flit_buffer->_output_endpoint_list->size() > 0,
            "Output Endpoint List Size(%u)", flit_buffer->_output_endpoint_list->size());

      if (!flit_buffer->_output_channels_allocated)
      {
         LOG_ASSERT_ERROR(flit->_type & Flit::HEAD, "flit->_type(%u)", flit->_type);

         // Get a pointer to the HeadFlit structure
         HeadFlit* head_flit = (HeadFlit*) flit;

         // Allocate both output channels and downstream buffers for the entire packet
         
         // Time at which downstream buffers are allocated
         UInt64 max_allocated_time = 0;
         
         // Try to allocate Channels first
         // Try to allocate downstream buffers next
         vector<Channel::Endpoint>::iterator endpoint_it = flit_buffer->_output_endpoint_list->begin();
         for( ; endpoint_it != flit_buffer->_output_endpoint_list->end(); endpoint_it ++)
         {
            Channel::Endpoint output_endpoint = *endpoint_it;
            SInt32 output_channel = output_endpoint._channel_id;
 
            LOG_PRINT("Output Endpoint(%i,%i)", output_endpoint._channel_id, output_endpoint._index);
            LOG_ASSERT_ERROR(_input_channels_allocated_vec[output_channel] != input_channel,
                  "Output Channel(%i) already allocated for input channel(%i)",
                  output_channel, input_channel);
            
            // Try to allocate channels
            if (_input_channels_allocated_vec[output_channel] != Channel::INVALID)
            {
               // Flits of some other flow are occupying the output channel to use
               LOG_PRINT("Output Channel(%i) already allocated to input channel(%i)",
                     output_channel, _input_channels_allocated_vec[output_channel]);
               return make_pair(false,false);
            }
            
            // Try to allocate downstream buffers
            UInt64 allocated_time = tryAllocateDownstreamBuffer(head_flit, output_endpoint, head_flit->_num_flits);
            if (allocated_time == UINT64_MAX_)
            {
               LOG_PRINT("Could not allocate a buffer for output endpoint(%i,%i): Num Flits(%i)",
                     output_endpoint._channel_id, output_endpoint._index, head_flit->_num_flits);
               return make_pair(false,false);
            }
            // Compute the maximum
            max_allocated_time = max<UInt64>(max_allocated_time, allocated_time);
         }
         // I can allocate all channels if I want to
         // I can allocate all downstream buffers if I want to

         // All output endpoints have a free buffer
         LOG_PRINT("Buffer allocated downstream for all output endpoints and flits: Allocated Time(%llu)", max_allocated_time);

         // Update Flit Time
         head_flit->_normalized_time = max<UInt64>(head_flit->_normalized_time, max_allocated_time);

         // Allocate all output channels and downstream buffers
         endpoint_it = flit_buffer->_output_endpoint_list->begin();
         for ( ; endpoint_it != flit_buffer->_output_endpoint_list->end(); endpoint_it ++)
         {
            Channel::Endpoint output_endpoint = *endpoint_it;
            SInt32 output_channel = output_endpoint._channel_id;

            LOG_PRINT("Allocating Output Channel(%i)", output_channel);
            _input_channels_allocated_vec[output_channel] = input_channel;
      
            LOG_PRINT("Allocating Downstream Buffer(%i,%i): Num Flits(%i)",
                  output_channel, output_endpoint._index, head_flit->_num_flits);
            allocateDownstreamBuffer(head_flit, output_endpoint, head_flit->_num_flits);
         }

         // Set _output_channels_allocated to true
         flit_buffer->_output_channels_allocated = true;

      } // (!flit_buffer->_output_channels_allocated)

      // Now, all resources have been allocated and the flit times have been updated
      // Send the flits to the output endpoints
      vector<Channel::Endpoint>::iterator endpoint_it = flit_buffer->_output_endpoint_list->begin();
      for ( ; (endpoint_it + 1) != flit_buffer->_output_endpoint_list->end(); endpoint_it ++)
      {
         Channel::Endpoint output_endpoint = *endpoint_it;

         NetPacket* cloned_net_packet = flit->_net_packet->clone();
         Flit* cloned_flit = (Flit*) cloned_net_packet->data;
         cloned_flit->_net_packet = cloned_net_packet;
         cloned_flit->_output_endpoint = output_endpoint;

         _network_msg_list->push_back(cloned_flit);
      }
      
      // Send the Flit to the last endpoint also
      flit->_output_endpoint = flit_buffer->_output_endpoint_list->back();
      _network_msg_list->push_back(flit);

   } // (head_flit->_receiver == NetPacket::BROADCAST)

   else // (head_flit->_receiver != NetPacket::BROADCAST)
   {
      LOG_ASSERT_ERROR(flit_buffer->_output_endpoint_list->size() == 1,
            "Got output_endpoint_list size(%u) for unicast packet", flit_buffer->_output_endpoint_list->size());

      Channel::Endpoint output_endpoint = flit_buffer->_output_endpoint_list->back();

      if (!flit_buffer->_output_channels_allocated)
      {
         // Allocate Output Channel
         LOG_ASSERT_ERROR(flit->_type & Flit::HEAD, "Not HEAD Flit: Type(0x%x)", flit->_type);
      
         SInt32 output_channel = output_endpoint._channel_id;
         LOG_ASSERT_ERROR(_input_channels_allocated_vec[output_channel] != input_channel,
               "Output Channel(%) already allocated to Input Channel(%i)",
               output_channel, input_channel);

         if (_input_channels_allocated_vec[output_channel] != Channel::INVALID)
         {
            LOG_PRINT("Output Channel(%i) already allocated to input channel(%i)",
                  output_channel, _input_channels_allocated_vec[output_channel]);
            return make_pair(false,false);
         }
         _input_channels_allocated_vec[output_channel] = input_channel;

         // Allocated output channels
         flit_buffer->_output_channels_allocated = true;
      }

      // Allocate downstream buffer - Allocate one flit at a time
      UInt64 allocated_time = tryAllocateDownstreamBuffer(flit, output_endpoint, 1);
      if (allocated_time == UINT64_MAX_)
      {
         LOG_PRINT("Could not allocate downstream buffer for output endpoint(%i,%i)",
               output_endpoint._channel_id, output_endpoint._index);
         return make_pair(false,false);
      }

      // Updating Flit Time
      flit->_normalized_time = max<UInt64>(flit->_normalized_time, allocated_time);
      
      LOG_PRINT("Allocating Downstream Buffer(%i,%i)", output_endpoint._channel_id, output_endpoint._index);
      allocateDownstreamBuffer(flit, output_endpoint, 1);

      // Allocated both channel and buffer, now send the flit

      LOG_PRINT("Adding to list of messages to send");
      // No need to clone. Just one flit
      flit->_output_endpoint = output_endpoint;
      _network_msg_list->push_back(flit);
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

   // If TAIL Flit, release all channels and set _output_channels_allocated to false
   // Delete output_endpoint_list also
   if (flit->_type & Flit::TAIL)
   {
      LOG_PRINT("TAIL flit");
      
      vector<Channel::Endpoint>::iterator endpoint_it = flit_buffer->_output_endpoint_list->begin();
      for ( ; endpoint_it != flit_buffer->_output_endpoint_list->end(); endpoint_it ++)
      {
         Channel::Endpoint output_endpoint = *endpoint_it;
         SInt32 output_channel = output_endpoint._channel_id;
         assert(_input_channels_allocated_vec[output_channel] == input_channel);
         _input_channels_allocated_vec[output_channel] = Channel::INVALID;
      }
      // Set Output Channels allocated to false
      flit_buffer->_output_channels_allocated = false;
      
      // All flits of the packet have been sent on all output channels
      delete flit_buffer->_output_endpoint_list;
      flit_buffer->_output_endpoint_list = NULL;
      
      LOG_PRINT("sendFlit(%i) exit->(true,true)", input_channel);
      return make_pair(true,true);
   }
   else
   {
      // There is at least one flit remaining to be sent on all output channels
      LOG_PRINT("sendFlit(%i) exit->(true,false)", input_channel);
      return make_pair<bool,bool>(true,false);
   }
}

void
WormholeUnicastVirtualCutThroughBroadcastFlowControlScheme::allocateDownstreamBuffer(Flit* flit, Channel::Endpoint& output_endpoint, SInt32 num_buffers)
{
   BufferStatusList* buffer_status_list = _vec_downstream_buffer_status_list[output_endpoint._channel_id];
   buffer_status_list->allocateBuffer(flit, output_endpoint._index, num_buffers);
}

UInt64
WormholeUnicastVirtualCutThroughBroadcastFlowControlScheme::tryAllocateDownstreamBuffer(Flit* flit, Channel::Endpoint& output_endpoint, SInt32 num_buffers)
{
   BufferStatusList* buffer_status_list = _vec_downstream_buffer_status_list[output_endpoint._channel_id];
   return buffer_status_list->tryAllocateBuffer(flit, output_endpoint._index, num_buffers);
}
