#pragma once

#include <vector>
using namespace std;

#include "flow_control_scheme.h"
#include "buffer_management_scheme.h"

class RouterPerformanceModel
{
   public:
      RouterPerformanceModel( \
            FlowControlScheme::Type flow_control_scheme, \
            SInt32 data_pipeline_delay, \
            SInt32 credit_pipeline_delay, \
            SInt32 num_input_channels, SInt32 num_output_channels, \
            vector<SInt32>& num_input_endpoints_list, \
            vector<SInt32>& num_output_endpoints_list, \
            vector<BufferManagementScheme::Type>& input_buffer_management_schemes, \
            vector<BufferManagementScheme::Type>& downstream_buffer_management_schemes, \
            vector<SInt32>& input_buffer_size_list, \
            vector<SInt32>& downstream_buffer_size_list);
      ~RouterPerformanceModel();

      void processDataMsg(Flit* flit, vector<NetworkMsg*>& network_msg_list_to_send);
      void processBufferManagementMsg(BufferManagementMsg* buffer_msg, \
            vector<NetworkMsg*>& network_msg_list_to_send);

      SInt32 getDataPipelineDelay() { return _data_pipeline_delay; }
      SInt32 getCreditPipelineDelay() { return _credit_pipeline_delay; }

      FlowControlScheme* getFlowControlObject()
      { return _flow_control_object; }

   private:
      FlowControlScheme* _flow_control_object;
      SInt32 _data_pipeline_delay;
      SInt32 _credit_pipeline_delay;
};
