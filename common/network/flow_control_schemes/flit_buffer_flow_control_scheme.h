#pragma once

#include "flow_control_scheme.h"

class FlitBufferFlowControlScheme : public FlowControlScheme
{
   public:
      FlitBufferFlowControlScheme(UInt32 num_input_channels, UInt32 num_output_channels, UInt32 input_queue_size);
      ~FlitBufferFlowControlScheme();
};
