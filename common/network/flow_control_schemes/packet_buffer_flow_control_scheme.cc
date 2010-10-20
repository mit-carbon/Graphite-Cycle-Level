#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include "packet_buffer_flow_control_scheme.h"

PacketBufferFlowControlScheme::PacketBufferFlowControlScheme(UInt32 num_input_channels, UInt32 num_output_channels, UInt32 input_queue_size):
   FlowControlScheme(num_input_channels, num_output_channels, input_queue_size)
{
   // Input Queueing assumed
   _list_of_input_queues.resize(_num_input_channels);
   _list_of_credit_histories.resize(_num_output_channels);
   _list_of_link_times.resize(_num_output_channels);

}

PacketBufferFlowControlScheme::~PacketBufferFlowControlScheme()
{}

void
PacketBufferFlowControlScheme::processDataMsg(Flit* flit, UInt32 input_channel)
{
   assert(flit->_type == Flit::HEAD);
   HeadFlit& head_flit = *((HeadFlit*) flit);
   
   assert(head_flit._length <= _input_queue_size);
   HeadFlitQueue& head_flit_queue = _list_of_input_queues[_input_channel];
   head_flit_queue.push(head_flit);

   iterate();
}

void
PacketBufferFlowControlScheme::processFlowControlMsg(FlowControlMsg* flow_control_msg, UInt32 channel)
{
   // Only credit-based backpressure scheme can be used with Store-and-forward flow control
   assert(flow_control_msg->_type == FlowControlMsg::CREDIT);
   CreditFlowControlMsg& credit_msg = *((CreditFlowControlMsg*) flow_control_msg);

   CreditHistory& credit_history = _list_of_credit_histories[channel];
   CreditStatus& latest_credit_status = credit_history.back();
   
   if (latest_credit_status._time < credit_msg._time)
   {
      // Add the credits to the end of the history
      for (UInt32 i = 0; i < credit_msg._num_credits; i++)
      {
         credit_history.push_back(CreditStatus(credit_msg._time + i, latest_credit_status._num_credits + i + 1));
      }
   }
   else
   {
      assert(credit_history.size() == 1);
      for (UInt32 i = 0; i < num_credits; i++)
      {
         if (credit_status_last._time == (credit_msg._time + i))
            credit_status_last._num_credits += (i + 1);
         else if (credit_status_last._time < (credit_msg._time + i))
            credit_history.push_back(CreditStatus(credit_msg._time + i, credit_status_last._num_credits + i + 1));
      }
      if (credit_status_last._time >= (credit_msg._time + num_credits))
         credit_status_last._num_credits += num_credits;
   }
 
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
   
   do
   {
      // Process all the input channels in FIFO order
      bool processing_finished = true;
      for (UInt32 input_channel = 0; input_channel < _num_input_channels; input_channel++)
      {
         HeadFlitQueue& head_flit_queue = _list_of_input_queues[input_channel];
         if (! head_flit_queue.empty())
         {
            bool sent = send(head_flit_queue.front());
            if (sent)
            {
               head_flit_queue.pop();
               processing_finished = false;
            }
         }
      }
   } while (! processing_finished);
}

bool
PacketBufferFlowControlScheme::send(HeadFlit& head_flit)
{
   vector<CreditStatus>& credit_history = _list_of_credit_histories[head_flit._output_channel];
   vector<CreditStatus>::iterator it;
   for (it = credit_history.begin(); it != credit_history.end(); it++)
   {
      if ((*it)._num_credits >= head_flit._length)
      {
         // Computes the HoL blocking delay
         HeadFlitQueue& head_flit_queue = _list_of_input_queues[head_flit._input_channel];
         UInt64 head_of_line_blocking_delay = (head_flit_queue._time > head_flit._time) ? \
                                              (head_flit_queue._time - head_flit._time) : 0;
         head_flit._time += head_of_line_blocking_delay;

         // Computes the buffer allocation delay
         UInt64 buffer_allocation_queue_delay = ((*it)._time > head_flit._time) ? \
                                                ((*it)._time - head_flit._time) : 0;
         head_flit._time += buffer_allocation_queue_delay;
        
         // Computes the link contention delay
         UInt64& link_time = _list_of_link_times[head_flit._output_channel];
         UInt64 link_allocation_queue_delay = (link_time > head_flit._time) ? (link_time - head_flit._time) : 0;
         head_flit._time += link_allocation_queue_delay;

         // To model HoL blocking: Update input queue time
         head_flit_queue._time = head_flit._time + head_flit._length;

         // To model finite buffers
         // Decrease credits from downstream credit History
         updateCreditHistory(head_flit._output_channel, head_flit._time + head_flit._length, head_flit._length);

         // To model Link contention: Update link time
         link_time = head_flit._time + head_flit._length;
         
         // Send Packet to downstream router
         _data_msg_list.push_back(head_flit);

         // Send credits back to upstream router
         _credit_msg_list.push_back(CreditMsg(head_flit._input_channel, head_flit._time, head_flit._length));

         // Sending of the packet is successful
         return true;
      }
   }
   // Did not send the Packet
   return false;
}

void
PacketBufferFlowControlScheme::updateCreditHistory(UInt32 output_channel, UInt64 time, UInt32 credits_lost)
{
   list<CreditStatus>& credit_history = _list_of_credit_histories[output_channel];
   list<CreditStatus>::iterator it;
   for (it = credit_history.begin(); it != credit_history.end(); it++)
   {
      if ((*it)._time > time)
         break;
   }

   // Cannot be the oldest credit status
   assert(it != credit_history.begin());

   UInt32 credits = (*(it-1))._num_credits;
   // Delete the credit status history till then
   it = credit_history.erase(credit_history.begin(), it);
   // Insert current credits
   credit_history.insert(it, CreditStatus(time, credits - credits_lost));
   // Propagate changes to other entries in the credit history
   for ( ; it != credit_history.end(); it++)
   {
      (*it)._num_credits -= credits_lost;
   }
}
void
PacketBufferFlowControlScheme::normalize()
{
   // Calculates the reference time for the packet - normalizes it w.r.t
   // clocks of other cores
}
