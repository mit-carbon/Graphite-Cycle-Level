#pragma once

#include <list>
#include <vector>
#include <queue>
using namespace std;

#include "fixed_types.h"
#include "network_msg.h"
#include "flit.h"
#include "buffer_management_msg.h"
#include "flit_buffer_flow_control_scheme.h"

class WormholeFlowControlScheme : public FlitBufferFlowControlScheme
{
   private:
      class FlitQueue
      {
         private:
            struct EmptyFlitStatus
            {
               UInt32 _count;
               UInt64 _time;
            };
            queue<Flit*> _flit_queue;

         public:
            // To model HoL blocking
            UInt64 _time;
            // To model packets with many body flits
            UInt32 _output_channel;
            // Flits remaining in the packet
            UInt32 _flits_remaining_in_pkt;
            // (Number of Empty Flits, Last Update Time) in the queue
            EmptyFlitStatus _empty_flit_status;

            bool empty() { return _flit_queue.empty(); }
            UInt32 size() { return _flit_queue.size(); }
            void push(Flit* flit) { _flit_queue.push(flit); }
            void pop() { return _flit_queue.pop(); }
            Flit* front() { return _flit_queue.front(); }
            Flit* back() { return _flit_queue.back(); }
      };

      typedef list<BufferManagementMsg*> BufferUsageHistory;

      vector<FlitQueue> _list_of_input_queues;
      vector<BufferUsageHistory> _list_of_buffer_usage_histories;
      vector<UInt32> _input_channels_allocated;

      BufferManagementScheme::Type _upstream_buffer_management_scheme;
      BufferManagementScheme::Type _downstream_buffer_management_scheme;

      UInt32 _on_off_threshold;

      // Vector of NetworkMsg
      vector<NetworkMsg*> _network_msg_list;

      // Private Functions
      vector<NetworkMsg*>& processDataMsg(Flit* flit, UInt32 input_channel);
      vector<NetworkMsg*>& processBufferManagementMsg(BufferManagementMsg* buffer_management_msg, UInt32 channel);
      
      void iterate();
      pair<bool,bool> sendFlit(UInt32 input_channel);
      bool allocateDownstreamBuffer(Flit* flit, UInt32 output_channel);
      void updateDownstreamBufferUsageHistory(UInt32 output_channel, UInt64 time);
      void updateUpstreamBufferUsageHistory(UInt32 input_channel, UInt64 time, bool flit_send);

   public:
      WormholeFlowControlScheme(UInt32 num_input_channels, UInt32 num_output_channels, UInt32 input_queue_size, \
            string upstream_buffer_management_scheme, string downstream_buffer_management_scheme, UInt32 on_off_threshold = 0);
      ~WormholeFlowControlScheme();

      // Public Functions
      vector<NetworkMsg*>& processNetworkMsg(NetworkMsg* network_msg, UInt32 channel);
};
