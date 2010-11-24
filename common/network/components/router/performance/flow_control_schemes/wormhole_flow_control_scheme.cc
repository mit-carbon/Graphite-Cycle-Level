#include "wormhole_flow_control_scheme.h"
#include "log.h"

WormholeFlowControlScheme::WormholeFlowControlScheme( \
      SInt32 num_input_channels, SInt32 num_output_channels, \
      vector<SInt32>& num_input_endpoints_list, vector<SInt32>& num_output_endpoints_list, \
      vector<BufferManagementScheme::Type>& input_buffer_management_scheme_vec, \
      vector<BufferManagementScheme::Type>& downstream_buffer_management_scheme_vec, \
      vector<SInt32>& input_buffer_size_vec, vector<SInt32>& downstream_buffer_size_vec):
   FlitBufferFlowControlScheme(num_input_channels, num_output_channels)
{
   // Create Input Queues
   _input_flit_buffer_vec.resize(_num_input_channels);
   for (SInt32 i = 0; i < _num_input_channels; i++)
   {
      _input_flit_buffer_vec[i] = new FlitBuffer(input_buffer_management_scheme_vec[i], \
            input_buffer_size_vec[i]);
   }

   // Create downstream buffer usage histories
   _list_of_downstream_buffer_usage_histories_vec.resize(_num_output_channels);
   for (SInt32 i = 0; i < _num_output_channels; i++)
   {
      _list_of_downstream_buffer_usage_histories_vec[i] = new ListOfBufferUsageHistories( \
            num_output_endpoints_list[i], \
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
      delete _list_of_downstream_buffer_usage_histories_vec[i];
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
WormholeFlowControlScheme::processBufferManagementMsg( \
      BufferManagementMsg* buffer_management_msg, vector<NetworkMsg*>& network_msg_list)
{
   LOG_PRINT("processBufferManagementMsg(%p, %p) enter", this, buffer_management_msg);

   _network_msg_list = &network_msg_list;

   Channel::Endpoint& output_endpoint = buffer_management_msg->_output_endpoint;
   ListOfBufferUsageHistories* list_of_buffer_usage_histories = \
         _list_of_downstream_buffer_usage_histories_vec[output_endpoint._channel_id];
   list_of_buffer_usage_histories->receiveBufferManagementMsg( \
         buffer_management_msg, output_endpoint._index);
 
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

   LOG_PRINT("Got Flit(%p)", flit);

   if (flit_buffer->_output_endpoint_list == NULL)
   {
      LOG_PRINT("Head Flit");
      LOG_ASSERT_ERROR(flit->_type & Flit::HEAD, "flit->_type(%u)", flit->_type);
      flit_buffer->_output_endpoint_list = flit->_output_endpoint_list;
   }

   // Get the curr output_endpoint
   flit->_output_endpoint = flit_buffer->_output_endpoint_list->curr();

   LOG_PRINT("Output Endpoint(%i,%i)", flit->_output_endpoint._channel_id, \
         flit->_output_endpoint._index);

   if (flit->_output_endpoint == flit_buffer->_output_endpoint_list->first())
   {
      LOG_PRINT("Update Flit Time");
      flit_buffer->updateFlitTime();
   }

   SInt32 output_channel = flit->_output_endpoint._channel_id;

   if ( (_input_channels_allocated_vec[output_channel] != Channel::INVALID) && \
      (_input_channels_allocated_vec[output_channel] != input_channel) )
   {
      // Flits of some other flow are occupying the output channel to use
      LOG_ASSERT_ERROR(flit->_type & Flit::HEAD, "flit->_type(%u)", flit->_type);
      
      LOG_PRINT("sendFlit(%i) exit->(false,false)", input_channel);
      return make_pair<bool,bool>(false,false);
   }
   else if (_input_channels_allocated_vec[output_channel] == Channel::INVALID)
   {
      LOG_PRINT("Channel::INVALID, allocating");

      LOG_ASSERT_ERROR(flit->_type & Flit::HEAD, "flit->_type(%u)", flit->_type);
      _input_channels_allocated_vec[output_channel] = input_channel;
   }

   // Model buffer allocation (at downstream router)
   // Also changes downstream buffer usage history
   bool allocated = allocateDownstreamBuffer(flit);
   if (allocated)
   {
      LOG_PRINT("Downstream Buffer allocated: Updating Buffer Time");

      // flit_to_send downstream
      Flit* flit_to_send;

      flit_buffer->updateBufferTime();
      if (flit->_output_endpoint == flit_buffer->_output_endpoint_list->last())
      {
         LOG_PRINT("Last Output Channel");

         // Remove flit from queue
         // Update upstream buffer usage history
         BufferManagementMsg* upstream_buffer_msg = flit_buffer->dequeue();
         if (upstream_buffer_msg)
         {
            LOG_PRINT("Sending Upstream Buffer Msg (%p)", upstream_buffer_msg);

            upstream_buffer_msg->_input_endpoint = flit->_input_endpoint;
            _network_msg_list->push_back(upstream_buffer_msg);
         }
         // Send flit downstream
         flit_to_send = flit;
      }
      else
      {
         LOG_PRINT("NOT Last Output Channel -> Duplicate Flit");

         // Duplicate flit and net_packet
         NetPacket* cloned_net_packet = flit->_net_packet->clone();
         Flit* cloned_flit = (Flit*) cloned_net_packet->data;
         cloned_flit->_net_packet = cloned_net_packet;
         
         // Send cloned_flit downstream
         flit_to_send = cloned_flit;
      }
      
      // Update ptr to _output_endpoint_list
      flit_buffer->_output_endpoint_list->incr();

      LOG_PRINT("Adding to list of messages to send");

      // Send flit to downstream router
      _network_msg_list->push_back(flit_to_send);

      if (flit->_type & Flit::TAIL)
      {
         LOG_PRINT("TAIL flit");

         _input_channels_allocated_vec[output_channel] = Channel::INVALID;
         if (flit->_output_endpoint == flit_buffer->_output_endpoint_list->last())
         {
            // All flits of packet have been sent on all output channels
            // Delete _output_endpoint_list
            delete flit_buffer->_output_endpoint_list;
            flit_buffer->_output_endpoint_list = NULL;
      
            LOG_PRINT("sendFlit(%i) exit->(true,true)", input_channel);
            return make_pair<bool,bool>(true,true);
         }
         else
         {
            LOG_PRINT("sendFlit(%i) exit->(true,false)", input_channel);
            // All flits of packet have been sent on at least one output channel
            return make_pair<bool,bool>(true,false);
         }
      }
      else
      {
         LOG_PRINT("sendFlit(%i) exit->(true,false)", input_channel);
         // There is at least one flit remaining to be sent on all output channels
         return make_pair<bool,bool>(true,false);
      }
   }
   else
   {
      LOG_PRINT("sendFlit(%i) exit->(false,false)", input_channel);
      return make_pair<bool,bool>(false,false);
   }
}

bool
WormholeFlowControlScheme::allocateDownstreamBuffer(Flit* flit)
{
   Channel::Endpoint& output_endpoint = flit->_output_endpoint;
   ListOfBufferUsageHistories* list_of_buffer_usage_histories = \
         _list_of_downstream_buffer_usage_histories_vec[output_endpoint._channel_id];
   return list_of_buffer_usage_histories->allocateBuffer(flit, output_endpoint._index);
}
