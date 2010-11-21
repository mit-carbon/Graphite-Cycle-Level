#include "virtual_cut_through_flow_control_scheme.h"

VirtualCutThroughFlowControlScheme::VirtualCutThroughFlowControlScheme( \
      SInt32 num_input_channels, SInt32 num_output_channels, \
      vector<SInt32>& num_input_endpoints_list, vector<SInt32>& num_output_endpoints_list, \
      vector<BufferManagementScheme::Type>& input_buffer_management_scheme_vec, \
      vector<BufferManagementScheme::Type>& downstream_buffer_management_scheme_vec, \
      vector<SInt32>& input_buffer_size_vec, vector<SInt32>& downstream_buffer_size_vec):
   PacketBufferFlowControlScheme(num_input_channels, num_output_channels, \
         num_input_endpoints_list, num_output_endpoints_list, \
         input_buffer_management_scheme_vec, downstream_buffer_management_scheme_vec, \
         input_buffer_size_vec, downstream_buffer_size_vec)
{}

VirtualCutThroughFlowControlScheme::~VirtualCutThroughFlowControlScheme()
{}
