#pragma once

#include "wormhole_flow_control_scheme.h"

class WormholeUnicastVirtualCutThroughBroadcastFlowControlScheme : public WormholeFlowControlScheme
{
   public:
      WormholeUnicastVirtualCutThroughBroadcastFlowControlScheme(SInt32 num_input_channels, SInt32 num_output_channels,
            vector<SInt32>& num_input_endpoints_list, vector<SInt32>& num_output_endpoints_list,
            vector<BufferManagementScheme::Type>& input_buffer_management_scheme_vec,
            vector<BufferManagementScheme::Type>& downstream_buffer_management_scheme_vec,
            vector<SInt32>& input_buffer_size_vec,
            vector<SInt32>& downstream_buffer_size_vec);
      ~WormholeUnicastVirtualCutThroughBroadcastFlowControlScheme();

   private:
      // Private Functions
      pair<bool,bool> sendFlit(SInt32 input_channel);
      void allocateDownstreamBuffer(Flit* flit, Channel::Endpoint& output_endpoint, SInt32 num_buffers);
      UInt64 tryAllocateDownstreamBuffer(Flit* flit, Channel::Endpoint& output_endpoint, SInt32 num_buffers);
};
