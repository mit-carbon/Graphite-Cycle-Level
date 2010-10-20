#pragma once

#include "packet_buffer_flow_control_scheme.h"

class StoreAndForwardFlowControlScheme : public PacketBufferFlowControlScheme
{
   public:
      StoreAndForwardFlowControlScheme(UInt32 num_input_channels, UInt32 num_output_channels, UInt32 input_queue_size);
      ~StoreAndForwardFlowControlScheme();
};
