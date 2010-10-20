#include "virtual_cut_through_flow_control_scheme.h"

VirtualCutThroughFlowControlScheme::VirtualCutThroughFlowControlScheme(UInt32 num_input_channels, UInt32 num_output_channels, UInt32 input_queue_size):
   PacketBufferFlowControlScheme(num_input_channels, num_output_channels, input_queue_size)
{}

VirtualCutThroughFlowControlScheme::~VirtualCutThroughFlowControlScheme()
{}
