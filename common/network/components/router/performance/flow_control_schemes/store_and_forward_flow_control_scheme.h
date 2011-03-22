#pragma once

#include "packet_buffer_flow_control_scheme.h"

class StoreAndForwardFlowControlScheme : public PacketBufferFlowControlScheme
{
   public:
      StoreAndForwardFlowControlScheme(SInt32 num_input_channels, SInt32 num_output_channels, \
            vector<SInt32>& num_input_endpoints_list, vector<SInt32>& num_output_endpoints_list, \
            vector<BufferManagementScheme::Type>& input_buffer_management_scheme_vec, \
            vector<BufferManagementScheme::Type>& downstream_buffer_management_scheme_vec, \
            vector<SInt32>& input_buffer_size_vec, vector<SInt32>& downstream_buffer_size_vec);
      ~StoreAndForwardFlowControlScheme();

      void processDataMsg(Flit *flit, vector<NetworkMsg*>& network_msg_list);
};