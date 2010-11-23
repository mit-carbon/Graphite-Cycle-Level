#pragma once

#include <vector>
#include <queue>
#include <list>
using namespace std;

#include "fixed_types.h"
#include "flit.h"
#include "head_flit.h"
#include "buffer_management_msg.h"
#include "buffer_model.h"
#include "list_of_buffer_usage_histories.h"
#include "flow_control_scheme.h"

class PacketBufferFlowControlScheme : public FlowControlScheme
{
   public:
      PacketBufferFlowControlScheme(SInt32 num_input_channels, SInt32 num_output_channels, \
            vector<SInt32>& num_input_endpoints_list, vector<SInt32>& num_output_endpoints_list, \
            vector<BufferManagementScheme::Type>& input_buffer_management_scheme_vec, \
            vector<BufferManagementScheme::Type>& downstream_buffer_management_scheme_vec, \
            vector<SInt32>& input_buffer_size_vec, vector<SInt32>& downstream_buffer_size_vec);
      ~PacketBufferFlowControlScheme();

      // Public Functions
      void processDataMsg(Flit* flit, vector<NetworkMsg*>& network_msg_list);
      void processBufferManagementMsg(BufferManagementMsg* buffer_management_msg, \
            vector<NetworkMsg*>& network_msg_list);

      // Dividing and coalescing packet at start and end
      static void dividePacket(NetPacket* net_packet, list<NetPacket*>& net_packet_list, \
            SInt32 num_flits);
      static bool isPacketComplete(NetPacket* net_packet);

      BufferModel* getBufferModel(SInt32 input_channel_id)
      { return _input_packet_buffer_vec[input_channel_id]; }
   
   private:
      typedef BufferModel PacketBuffer;

      vector<PacketBuffer*> _input_packet_buffer_vec;
      vector<ListOfBufferUsageHistories*> _list_of_downstream_buffer_usage_histories_vec;
      vector<NetworkMsg*>* _network_msg_list;

      // Private Functions
      void iterate();
      bool sendPacket(SInt32 input_channel);
      bool allocateDownstreamBuffer(HeadFlit* head_flit);
};
