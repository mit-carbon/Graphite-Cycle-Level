#pragma once

#include <vector>
#include <list>
using namespace std;

#include "network_model.h"
#include "network.h"
#include "router.h"
#include "head_flit.h"

class FiniteBufferNetworkModel : public NetworkModel
{
   public:
      FiniteBufferNetworkModel(Network* network, SInt32 network_id);
      ~FiniteBufferNetworkModel();

      // Send Network Packet
      void sendNetPacket(NetPacket* net_packet, list<NetPacket*>& net_packet_list_to_send);
      // Receive Network Packet
      void receiveNetPacket(NetPacket* net_packet, list<NetPacket*>& net_packet_list_to_send, \
            list<NetPacket*>& net_packet_list_to_receive);

   protected:
      // Core Id on which this object is present
      core_id_t _core_id;
      // Routers that are present on this core: There is a one-to-one mapping here
      vector<Router*> _router_list;
      // Flow Control Scheme
      FlowControlScheme::Type _flow_control_scheme;
      // Flit Width
      SInt32 _flit_width;

   private:
      // Sequence Number
      UInt64 _sequence_num;
      // Maps to store modeling and raw packets
      map<UInt64, NetPacket*> _received_raw_packet_map;
      map<UInt64, NetPacket*> _received_modeling_packet_map;
      // Flow Control Packet Type
      PacketType _flow_control_packet_type;

      // Compute the output endpoints->[channel, index] of a particular flit
      virtual void computeOutputEndpointList(HeadFlit* head_flit, Router* curr_router) = 0;

      // Construct NetPacket from NetworkMsg
      void constructNetPackets(Router* curr_router, vector<NetworkMsg*>& network_msg_list, \
            list<NetPacket*>& net_packet_list);
      // Receive raw packet containing actual application data (non-modeling packet)
      bool receiveRawPacket(NetPacket* raw_packet);
      // Receive modeling packet containing timing information (non-raw packet)
      NetPacket* receiveModelingPacket(NetPacket* modeling_packet);

      // Utils
      Flit* cloneFlit(Flit* flit);
      void addNetPacketEndpoints(NetPacket* net_packet, \
            Router::Id& sender_router_id, Router::Id& receiver_router_id);
};
