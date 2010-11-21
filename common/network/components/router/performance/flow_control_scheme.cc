#include "flow_control_scheme.h"
#include "store_and_forward_flow_control_scheme.h"
#include "virtual_cut_through_flow_control_scheme.h"
#include "wormhole_flow_control_scheme.h"
#include "log.h"

FlowControlScheme::Type
FlowControlScheme::parse(string flow_control_scheme_str)
{
   if (flow_control_scheme_str == "store_and_forward")
      return STORE_AND_FORWARD;
   else if (flow_control_scheme_str == "virtual_cut_through")
      return VIRTUAL_CUT_THROUGH;
   else if (flow_control_scheme_str == "wormhole")
      return WORMHOLE;
   else
   {
      LOG_PRINT_ERROR("Unrecognized Flow Control Scheme(%s)", \
            flow_control_scheme_str.c_str());
      return NUM_SCHEMES;
   }
}

FlowControlScheme*
FlowControlScheme::create(Type flow_control_scheme, \
      SInt32 num_input_channels, SInt32 num_output_channels, \
      vector<SInt32>& num_input_endpoints_list, vector<SInt32>& num_output_endpoints_list, \
      vector<BufferManagementScheme::Type>& input_buffer_management_schemes, \
      vector<BufferManagementScheme::Type>& downstream_buffer_management_schemes, \
      vector<SInt32>& input_buffer_size_list, \
      vector<SInt32>& downstream_buffer_size_list)
{
   switch(flow_control_scheme)
   {
      case STORE_AND_FORWARD:
         return new StoreAndForwardFlowControlScheme( \
               num_input_channels, num_output_channels, \
               num_input_endpoints_list, num_output_endpoints_list, \
               input_buffer_management_schemes, downstream_buffer_management_schemes, \
               input_buffer_size_list, downstream_buffer_size_list);

      case VIRTUAL_CUT_THROUGH:
         return new VirtualCutThroughFlowControlScheme( \
               num_input_channels, num_output_channels, \
               num_input_endpoints_list, num_output_endpoints_list, \
               input_buffer_management_schemes, downstream_buffer_management_schemes, \
               input_buffer_size_list, downstream_buffer_size_list);

      case WORMHOLE:
         return new WormholeFlowControlScheme( \
               num_input_channels, num_output_channels, \
               num_input_endpoints_list, num_output_endpoints_list, \
               input_buffer_management_schemes, downstream_buffer_management_schemes, \
               input_buffer_size_list, downstream_buffer_size_list);

      default:
         LOG_PRINT_ERROR("Unrecognized Flow Control Scheme(%u)", flow_control_scheme);
         return (FlowControlScheme*) NULL;
   }
}

void
FlowControlScheme::dividePacket(Type flow_control_scheme, \
      NetPacket* net_packet, list<NetPacket*>& net_packet_list, \
      SInt32 packet_length, SInt32 flit_width)
{
   switch (flow_control_scheme)
   {
      case STORE_AND_FORWARD:
      case VIRTUAL_CUT_THROUGH:
         PacketBufferFlowControlScheme::dividePacket(net_packet, net_packet_list, \
               packet_length, flit_width);
         break;

      case WORMHOLE:
         FlitBufferFlowControlScheme::dividePacket(net_packet, net_packet_list, \
               packet_length, flit_width);
         break;

      default:
         LOG_PRINT_ERROR("Unrecognized Flow Control Scheme(%u)", flow_control_scheme);
         break;
   }
}

bool
FlowControlScheme::isPacketComplete(Type flow_control_scheme, NetPacket* net_packet)
{
   switch (flow_control_scheme)
   {
      case STORE_AND_FORWARD:
      case VIRTUAL_CUT_THROUGH:
         return PacketBufferFlowControlScheme::isPacketComplete(net_packet);

      case WORMHOLE:
         return FlitBufferFlowControlScheme::isPacketComplete(net_packet);

      default:
         LOG_PRINT_ERROR("Unrecognized flow control scheme(%u)", flow_control_scheme);
         return true;
   }
}
