#pragma once

#include "packet_buffer_flow_control_scheme.h"

class VirtualCutThroughFlowControlScheme : public PacketBufferFlowControlScheme
{
   public:
      VirtualCutThroughFlowControlScheme(UInt32 num_input_channels, UInt32 num_output_channels, UInt32 input_queue_size);
      ~VirtualCutThroughFlowControlScheme();
};
