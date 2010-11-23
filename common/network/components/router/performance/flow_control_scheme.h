#pragma once

#include <vector>
#include <string>
using namespace std;

#include "buffer_management_scheme.h"
#include "fixed_types.h"
#include "network_msg.h"
#include "flit.h"
#include "buffer_management_msg.h"
#include "buffer_model.h"

class FlowControlScheme
{
   public:
      enum Type
      {
         STORE_AND_FORWARD = 0,
         VIRTUAL_CUT_THROUGH,
         WORMHOLE,
         NUM_SCHEMES
      };

      FlowControlScheme(SInt32 num_input_channels, SInt32 num_output_channels):
         _num_input_channels(num_input_channels),
         _num_output_channels(num_output_channels)
      {}
      virtual ~FlowControlScheme() {};

      static Type parse(string flow_control_scheme_str);
      
      static FlowControlScheme* create(Type flow_control_scheme, \
            SInt32 num_input_channels, SInt32 num_output_channels, \
            vector<SInt32>& num_input_endpoints_list, vector<SInt32>& num_output_endpoints_list, \
            vector<BufferManagementScheme::Type>& input_buffer_management_schemes, \
            vector<BufferManagementScheme::Type>& downstream_buffer_management_schemes, \
            vector<SInt32>& input_buffer_size_list, \
            vector<SInt32>& downstream_buffer_size_list);

      static void dividePacket(Type flow_control_scheme, \
            NetPacket* net_packet, list<NetPacket*>& net_packet_list, \
            SInt32 num_flits);
      static bool isPacketComplete(Type flow_control_scheme, NetPacket* net_packet);
      static SInt32 computeNumFlits(SInt32 packet_length, SInt32 flit_width);

      // Public Member Functions
      virtual void processDataMsg(Flit* flit, vector<NetworkMsg*>& network_msg_list) = 0;
      virtual void processBufferManagementMsg(BufferManagementMsg* buffer_management_msg, \
            vector<NetworkMsg*>& network_msg_list) = 0;

      virtual BufferModel* getBufferModel(SInt32 input_channel_id) = 0;

   protected:
      SInt32 _num_input_channels;
      SInt32 _num_output_channels;
};
