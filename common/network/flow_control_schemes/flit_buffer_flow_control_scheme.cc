#include "flit_buffer_flow_control_scheme.h"

FlitBufferFlowControlScheme::FlitBufferFlowControlScheme(UInt32 num_input_channels, \
      UInt32 num_output_channels, UInt32 input_queue_size):
   FlowControlScheme(num_input_channels, num_output_channels, input_queue_size)
{}

FlitBufferFlowControlScheme::~FlitBufferFlowControlScheme()
{}
