#pragma once

#include <vector>
#include <queue>
#include <list>
using namespace std;

#include "fixed_types.h"
#include "head_flit.h"
#include "flow_control_scheme.h"

class PacketBufferFlowControlScheme : public FlowControlScheme
{
   public:
      PacketBufferFlowControlScheme(UInt32 num_input_channels, UInt32 num_output_channels, UInt32 _input_queue_size);
      ~PacketBufferFlowControlScheme();

   private:
      class HeadFlitQueue
      {
         private:
            queue<HeadFlit> _head_flit_queue;

         public:
            // To model HoL blocking
            UInt64 _time;

            bool empty() { return _head_flit_queue.empty(); }
            UInt32 size() { return _head_flit_queue.size(); }
            void push(HeadFlit& head_flit) { _head_flit_queue.push(head_flit); }
            void pop() { _head_flit_queue.pop(); }
            HeadFlit& front() { return _head_flit_queue.front(); }
            HeadFlit& back() { return _head_flit_queue.back(); }
      };

      class CreditStatus
      {
         public:
            UInt64 _time;
            UInt32 _num_credits;
      };
      typedef list<CreditStatus> CreditHistory;

      vector<HeadFlitQueue> _list_of_input_queues;
      vector<CreditHistory> _list_of_credit_histories;
      vector<UInt64> _list_of_link_times; 
};
