#include "flow_control_scheme.h"
#include "store_and_forward_flow_control_scheme.h"
#include "virtual_cut_through_flow_control_scheme.h"
#include "wormhole_flow_control_scheme.h"
#include "wormhole_unicast__virtual_cut_through_broadcast__flow_control_scheme.h"
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
   else if (flow_control_scheme_str == "wormhole_unicast__virtual_cut_through_broadcast")
      return WORMHOLE_UNICAST__VIRTUAL_CUT_THROUGH_BROADCAST;
   else
   {
      LOG_PRINT_ERROR("Unrecognized Flow Control Scheme(%s)", flow_control_scheme_str.c_str());
      return NUM_SCHEMES;
   }
}

FlowControlScheme*
FlowControlScheme::create(Type flow_control_scheme,
      SInt32 num_input_channels, SInt32 num_output_channels,
      vector<SInt32>& num_input_endpoints_list, vector<SInt32>& num_output_endpoints_list,
      vector<BufferManagementScheme::Type>& input_buffer_management_schemes,
      vector<BufferManagementScheme::Type>& downstream_buffer_management_schemes,
      vector<SInt32>& input_buffer_size_list,
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

      case WORMHOLE_UNICAST__VIRTUAL_CUT_THROUGH_BROADCAST:
         return new WormholeUnicastVirtualCutThroughBroadcastFlowControlScheme( \
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
FlowControlScheme::dividePacket(Type flow_control_scheme,
                                NetPacket* net_packet, list<NetPacket*>& net_packet_list,
                                SInt32 serialization_latency)
{
   LOG_PRINT("dividePacket(FLOW_CONTROL_SCHEME - %u) enter", flow_control_scheme);
   switch (flow_control_scheme)
   {
      case STORE_AND_FORWARD:
      case VIRTUAL_CUT_THROUGH:
         PacketBufferFlowControlScheme::dividePacket(net_packet, net_packet_list, serialization_latency);
         break;

      case WORMHOLE:
      case WORMHOLE_UNICAST__VIRTUAL_CUT_THROUGH_BROADCAST:
         FlitBufferFlowControlScheme::dividePacket(net_packet, net_packet_list, serialization_latency);
         break;

      default:
         LOG_PRINT_ERROR("Unrecognized Flow Control Scheme(%u)", flow_control_scheme);
         break;
   }
   LOG_PRINT("dividePacket(FLOW_CONTROL_SCHEME - %u) exit", flow_control_scheme);
}

bool
FlowControlScheme::isPacketComplete(Type flow_control_scheme, Flit::Type flit_type)
{
   switch (flow_control_scheme)
   {
      case STORE_AND_FORWARD:
      case VIRTUAL_CUT_THROUGH:
         return PacketBufferFlowControlScheme::isPacketComplete(flit_type);

      case WORMHOLE:
      case WORMHOLE_UNICAST__VIRTUAL_CUT_THROUGH_BROADCAST:
         return FlitBufferFlowControlScheme::isPacketComplete(flit_type);

      default:
         LOG_PRINT_ERROR("Unrecognized flow control scheme(%u)", flow_control_scheme);
         return true;
   }
}
