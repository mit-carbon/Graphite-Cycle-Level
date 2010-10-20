#include "wormhole_flow_control_scheme.h"
#include "log.h"

WormholeFlowControlScheme::WormholeFlowControlScheme(UInt32 num_input_channels, UInt32 num_output_channels, \
      UInt32 input_queue_size, \
      string upstream_buffer_management_scheme, string downstream_buffer_management_scheme, \
      UInt32 on_off_threshold):
   FlitBufferFlowControlScheme(num_input_channels, num_output_channels, input_queue_size),
   _on_off_threshold(on_off_threshold)
{
   _upstream_buffer_management_scheme = BufferManagementScheme::parse(upstream_buffer_management_scheme);
   _downstream_buffer_management_scheme = BufferManagementScheme::parse(downstream_buffer_management_scheme);

   _list_of_input_queues.resize(_num_input_channels);
   _list_of_buffer_usage_histories.resize(_num_output_channels);
   _input_channels_allocated.resize(_num_output_channels);
}

WormholeFlowControlScheme::~WormholeFlowControlScheme()
{}

vector<NetworkMsg*>&
WormholeFlowControlScheme::processNetworkMsg(NetworkMsg* network_msg, UInt32 channel)
{
   switch (network_msg->_type)
   {
      case NetworkMsg::DATA:
         return processDataMsg(dynamic_cast<Flit*>(network_msg), channel);

      case NetworkMsg::BUFFER_MANAGEMENT:
         return processBufferManagementMsg(dynamic_cast<BufferManagementMsg*>(network_msg), channel);

      default:
         LOG_PRINT_ERROR("Unrecognized Message Type(%u)", network_msg->_type);
         return _network_msg_list;
   }
}

vector<NetworkMsg*>&
WormholeFlowControlScheme::processDataMsg(Flit* flit, UInt32 input_channel)
{
   FlitQueue& flit_queue = _list_of_input_queues[input_channel];
   flit_queue.push(flit);

   // Received a flit from the upstream router
   updateUpstreamBufferUsageHistory(input_channel, flit->_time, false);

   iterate();
   return _network_msg_list;
}

vector<NetworkMsg*>&
WormholeFlowControlScheme::processBufferManagementMsg(BufferManagementMsg* buffer_management_msg, UInt32 output_channel)
{
   BufferUsageHistory& buffer_usage_history = _list_of_buffer_usage_histories[output_channel];
   BufferManagementMsg* latest_buffer_status = buffer_usage_history.back();

   switch (_downstream_buffer_management_scheme)
   {
      case BufferManagementScheme::CREDIT:
     
         CreditBufferManagementMsg* credit_msg = dynamic_cast<CreditBufferManagementMsg*>(buffer_management_msg);
         CreditBufferManagementMsg* latest_credit_status = dynamic_cast<CreditBufferManagementMsg*>(latest_buffer_status);
         assert(credit_msg->_num_credits == 1);
         
         if (latest_credit_status->_time < credit_msg->_time)
         {
            buffer_usage_history.push_back(new CreditBufferManagementMsg(credit_msg->_time, \
                     latest_credit_status->_num_credits + 1));
         }
         else
         {
            assert(buffer_usage_history.size() == 1);
            latest_credit_status->_num_credits ++;
         }

         break;

      case BufferManagementScheme::ON_OFF:
     
         OnOffBufferManagementMsg* msg = dynamic_cast<OnOffBufferManagementMsg*>(buffer_management_msg);
         OnOffBufferManagementMsg* latest_status = dynamic_cast<OnOffBufferManagementMsg*>(latest_buffer_status);
         assert(msg->_on_off_status != latest_status->_on_off_status);
         
         if (latest_status->_time < msg->_time)
         {
            buffer_usage_history.push_back(new OnOffBufferManagementMsg(msg->_time, msg->_on_off_status));
         }
         else if (latest_status->_time == msg->_time)
         {
            if (buffer_usage_history.size() == 1)
            {
               // Set the on-off status of the last element correctly
               latest_status->_on_off_status = msg->_on_off_status;
            }
            else
            {
               // Erase the last element
               buffer_usage_history.erase(buffer_usage_history.end() - 1);
            }
         }
         else // latest_on_off_status->_time > on_off_msg->_time
         {
            assert(buffer_usage_history.size() == 1);
            latest_status->_on_off_status = msg->_on_off_status;
         }
            
         break;

         default:
            LOG_PRINT_ERROR("Unrecognized Buffer Management Scheme (%u)", _downstream_buffer_management_scheme);
            break;
      }
   }

   iterate();
   return _network_msg_list;
}

void
WormholeFlowControlScheme::iterate()
{
   do
   {
      // Process all the input channels in FIFO order
      bool processing_finished = true;
      for (UInt32 input_channel = 0; input_channel < _num_input_channels; input_channel++)
      {
         FlitQueue& flit_queue = _list_of_input_queues[input_channel];
         bool done = false;
         while (!done)
         {
            pair<bool,bool> sent = sendFlit(input_channel);
            if (sent.first && sent.second) // flit_sent && packet_sent
            {
               flit_queue.pop();
               done = true;
               processing_finished = false;
            }
            else if (sent.first && !sent.second) // flit_sent && !packet_sent
            {
               flit_queue.pop();
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
      } // for (UInt32 input_channel = 0; input_channel < _num_input_channels; input_channel++)
   } while (! processing_finished);
}

// ret.first : true if sending flit is successful
// ret.second : true if sending packet is successful
pair<bool, bool>
WormholeFlowControlScheme::sendFlit(UInt32 input_channel)
{
   FlitQueue& flit_queue = _list_of_input_queues[input_channel];
   if (flit_queue.empty())
   {
      return make_pair<bool,bool>(false,false);
   }

   Flit* flit = flit_queue.front();
   if (flit->_type == Flit::HEAD)
   {
      assert(flit_queue._flits_remaining_in_pkt == 0);
      HeadFlit* head_flit = dynamic_cast<HeadFlit*>(flit);
      flit_queue._output_channel = head_flit->_output_channel;
      flit_queue._flits_remaining_in_pkt = head_flit->_length;
   }
   else
   {
      assert(flit_queue._flits_remaining_in_pkt > 0);
   }

   if ( (_input_channels_allocated[flit_queue._output_channel] != INVALID_CHANNEL) && \
      (_input_channels_allocated[flit_queue._output_channel] != input_channel) )
   {
      return make_pair<bool,bool>(false,false);
   }
   else if (_input_channels_allocated[flit_queue._output_channel] == INVALID_CHANNEL)
   {
      _input_channels_allocated[flit_queue._output_channel] = input_channel;
   }

   // Model HoL delay
   UInt64 head_of_line_blocking_delay = (flit_queue._time > flit->_time) ? \
                                        (flit_queue._time - flit->_time) : 0;
   flit->_time += head_of_line_blocking_delay;

   // Model buffer allocation (at downstream router) delay
   bool allocated = allocateDownstreamBuffer(flit, flit_queue._output_channel); 
   if (allocated)
   {
      // Update HoL (flit_queue) time
      flit_queue._time = flit->_time + 1;

      // To model finite buffers
      // Decrease credits / change on-off status from downstream credit history
      updateDownstreamBufferUsageHistory(flit_queue._output_channel, flit->_time);

      // Send flit to downstream router
      _network_msg_list.push_back(flit);

      // Send credits / toggle on-off status to upstream router
      updateUpstreamBufferUsageHistory(input_channel, flit->_time, true);

      // Decrement the number of flits expected in the packet
      flit_queue._flits_remaining_in_pkt --;

      if (flit_queue._flits_remaining_in_pkt == 0)
      {
         _input_channels_allocated[flit_queue._output_channel] = INVALID_CHANNEL;
         return make_pair<bool,bool>(true,true);
      }
      else
      {
         return make_pair<bool,bool>(true,false);
      }
   }
   else
   {
      return make_pair<bool,bool>(false,false);
   }
}

bool
WormholeFlowControlScheme::allocateDownstreamBuffer(Flit* flit, UInt32 output_channel)
{
   BufferUsageHistory& buffer_usage_history = _list_of_buffer_usage_histories[output_channel];
   BufferUsageHistory::iterator it = buffer_usage_history.begin();
   assert(it != buffer_usage_history.end());

   switch (_downstream_buffer_management_scheme)
   {
      case BufferManagementScheme::CREDIT:
         CreditBufferManagementMsg* credit_msg = dynamic_cast<CreditBufferManagementMsg*>(*it);
         if (credit_msg->_num_credits > 0)
         {
            UInt64 buffer_allocation_delay = (credit_msg->_time > flit->_time) ? \
                                             (credit_msg->_time - flit->_time) : 0;
            flit->_time += buffer_allocation_delay;

            return true;
         }
         else
         {
            assert(buffer_usage_history.size() == 1);
            return false;
         }

      case BufferManagementScheme::ON_OFF:
         for ( ; it != buffer_usage_history.end(); it ++)
         {
            OnOffBufferManagementMsg* curr_on_off_msg = dynamic_cast<OnOffBufferManagementMsg*>(*it);
            if (curr_on_off_msg->_on_off_status)
            {
               if ( (curr_on_off_msg->_time >= flit->_time) || ((it+1) == buffer_usage_history.end()) )
               {
                  UInt64 buffer_allocation_delay = (curr_on_off_msg->_time > flit->_time) ? \
                                                   (curr_on_off_msg->_time - flit->_time) : 0;
                  flit->_time += buffer_allocation_delay;
                  return true;
               }
               else // (curr_on_off_msg->_time < flit->_time) && ((it+1) != buffer_usage_history.end())
               {
                  OnOffBufferManagementMsg* next_on_off_msg = dynamic_cast<OnOffBufferManagementMsg*>(*(it+1));
                  assert(!next_on_off_msg->_on_off_status);
                  if (next_on_off_msg->_time > flit->_time)
                  {
                     // No buffer allocation delay
                     return true;
                  }
               }
            }
         }
         
         return false;

      default:
         LOG_PRINT_ERROR("Unrecognized Buffer Management Scheme(%u)", _downstream_buffer_management_scheme);
         return false;
   }
}

void
WormholeFlowControlScheme::updateDownstreamBufferUsageHistory(UInt32 output_channel, UInt64 time)
{
   BufferUsageHistory& buffer_usage_history = _list_of_buffer_usage_histories[output_channel];
   switch (_downstream_buffer_management_scheme)
   {
      case BufferManagementScheme::CREDIT:
         BufferUsageHistory::iterator it;
         for (it = buffer_usage_history.begin(); it != buffer_usage_history.end(); it ++)
         {
            CreditBufferManagementMsg* credit_msg = dynamic_cast<CreditBufferManagementMsg*>(*it);
            if (credit_msg->_time > time)
               break;
         }
         // Cannot be the oldest credit usage
         assert(it != buffer_usage_history.begin());

         UInt32 credits = dynamic_cast<CreditBufferManagementMsg*>(*(it-1))->_num_credits;
         assert(credits > 0);
         
         // Delete the credit history till then
         for (BufferUsageHistory::iterator it2 = buffer_usage_history.begin(); it2 != it; it2 ++)
         {
            delete (*it2);
         }
         it = buffer_usage_history.erase(buffer_usage_history.begin(), it);
         
         // Insert current credits
         buffer_usage_history.insert(it, new CreditBufferManagementMsg(time, credits-1));
         
         // Propagate credit decrease to other entries in the credit history
         for ( ; it != buffer_usage_history.end(); it++)
         {
            credit_msg = dynamic_cast<CreditBufferManagementMsg*>(*it);
            credit_msg->_num_credits --;
         }

         break;

      case BufferManagementScheme::ON_OFF:
         // Remove the history before the current flit
         BufferUsageHistory::iterator it = buffer_usage_history.begin();
         for ( ; it != buffer_usage_history.end(); it ++)
         {
            OnOffBufferManagementMsg* on_off_msg = dynamic_cast<OnOffBufferManagementMsg*>(*it);
            if (on_off_msg->_time > time)
               break;
         }
         
         // Cannot be the oldest credit status
         assert (it != buffer_usage_history.begin());

         bool on_off_status = dynamic_cast<OnOffBufferManagementMsg*>(*(it-1))->_on_off_status;

         // Delete the buffer status history till then
         for (BufferUsageHistory::iterator it2 = buffer_usage_history.begin(); it2 != it; it2 ++)
         {
            delete (*it2);
         }
         it = buffer_usage_history.erase(buffer_usage_history.begin(), it);

         // Insert current on-off status
         buffer_usage_history.insert(it, new OnOffBufferManagementMsg(time, on_off_status));

         break;

      default:
         LOG_PRINT_ERROR("Unrecognized Buffer Management Scheme (%u)", _downstream_buffer_management_scheme);
         break;
   }
}

void
WormholeFlowControlScheme::updateUpstreamBufferUsageHistory(UInt32 input_channel, UInt64 time, bool flit_send)
{
   switch (_upstream_buffer_management_scheme)
   {
      case BufferManagementScheme::CREDIT:
         if (flit_send)
            _network_msg_list.push_back(new CreditBufferManagementMsg(time, 1));
         break;

      case BufferManagementScheme::ON_OFF:
         FlitQueue& flit_queue = _list_of_input_queues[input_channel];
         
         // Synchronize 'time' to that of the _empty_flit_status
         // +1 to simplify processing at the upstream router
         UInt64 delay = (flit_queue._empty_flit_status._time > time) ? \
                        (flit_queue._empty_flit_status._time - time) : 0;
         time += delay;

         if (flit_send)
         {
            flit_queue._empty_flit_status._count ++;
            if (flit_queue._empty_flit_status._count == _on_off_threshold)
            {
               _network_msg_list.push_back(new OnOffBufferManagementMsg(time, true));
            }
         }
         else // (! flit_send) -> flit_receive
         {
            if (flit_queue._empty_flit_status._count == _on_off_threshold)
            {
               _network_msg_list.push_back(new OnOffBufferManagementMsg(time, false));
            }
            flit_queue._empty_flit_status._count --;
         }
         
         // Update empty flit status time
         flit_queue._empty_flit_status._time = time;
         break;

      default:
         LOG_PRINT_ERROR("Unrecognized Buffer Management Scheme (%u)", _upstream_buffer_management_scheme);
         break;
   }
}
