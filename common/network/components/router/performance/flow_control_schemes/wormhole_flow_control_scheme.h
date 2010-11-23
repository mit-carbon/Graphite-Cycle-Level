#pragma once

#include <vector>
using namespace std;

#include "fixed_types.h"
#include "flit.h"
#include "buffer_management_msg.h"
#include "list_of_buffer_usage_histories.h"
#include "flit_buffer_flow_control_scheme.h"

class WormholeFlowControlScheme : public FlitBufferFlowControlScheme
{
   public:
      WormholeFlowControlScheme(SInt32 num_input_channels, SInt32 num_output_channels, \
            vector<SInt32>& num_input_endpoints_list, vector<SInt32>& num_output_endpoints_list, \
            vector<BufferManagementScheme::Type>& input_buffer_management_scheme_vec, \
            vector<BufferManagementScheme::Type>& downstream_buffer_management_scheme_vec, \
            vector<SInt32>& input_buffer_size_vec, \
            vector<SInt32>& downstream_buffer_size_vec);
      ~WormholeFlowControlScheme();

      // Public Functions
      void processDataMsg(Flit* flit, vector<NetworkMsg*>& network_msg_list);
      void processBufferManagementMsg(BufferManagementMsg* buffer_msg, \
            vector<NetworkMsg*>& network_msg_list);

      BufferModel* getBufferModel(SInt32 input_channel_id)
      { return _input_flit_buffer_vec[input_channel_id]->getBufferModel(); }
   
   private:
      // Private Fields
      vector<FlitBuffer*> _input_flit_buffer_vec;
      vector<ListOfBufferUsageHistories*> _list_of_downstream_buffer_usage_histories_vec;
      vector<SInt32> _input_channels_allocated_vec;
      vector<NetworkMsg*>* _network_msg_list;

      // Private Functions
      void iterate();
      pair<bool,bool> sendFlit(SInt32 input_channel);
      bool allocateDownstreamBuffer(Flit* flit);
};
