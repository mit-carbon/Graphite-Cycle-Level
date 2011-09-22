#include "router_performance_model.h"

RouterPerformanceModel::RouterPerformanceModel(
      FlowControlScheme::Type flow_control_scheme,
      SInt32 data_pipeline_delay,
      SInt32 credit_pipeline_delay,
      SInt32 num_input_channels, SInt32 num_output_channels,
      vector<SInt32> num_input_endpoints_list,
      vector<SInt32> num_output_endpoints_list,
      vector<BufferManagementScheme::Type> input_buffer_management_schemes,
      vector<BufferManagementScheme::Type> downstream_buffer_management_schemes,
      vector<SInt32> input_buffer_size_list,
      vector<SInt32> downstream_buffer_size_list)
   : _data_pipeline_delay(data_pipeline_delay)
   , _credit_pipeline_delay(credit_pipeline_delay)
{
   _flow_control_object = FlowControlScheme::create(flow_control_scheme,
         num_input_channels, num_output_channels,
         num_input_endpoints_list, num_output_endpoints_list,
         input_buffer_management_schemes, downstream_buffer_management_schemes,
         input_buffer_size_list, downstream_buffer_size_list);
}

RouterPerformanceModel::~RouterPerformanceModel()
{
   delete _flow_control_object;
}

void
RouterPerformanceModel::processDataMsg(Flit* flit,
      vector<NetworkMsg*>& network_msg_list_to_send)
{
   _flow_control_object->processDataMsg(flit, network_msg_list_to_send);
}

void
RouterPerformanceModel::processBufferManagementMsg(BufferManagementMsg* buffer_msg,
      vector<NetworkMsg*>& network_msg_list_to_send)
{
   _flow_control_object->processBufferManagementMsg(buffer_msg, network_msg_list_to_send);
}
