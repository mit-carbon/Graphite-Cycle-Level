#pragma once

#include <map>
#include <vector>
#include <list>
using namespace std;

#include "network_model.h"
#include "network.h"
#include "network_node.h"
#include "lock.h"
#include "head_flit.h"
#include "queue_model_simple.h"

class FiniteBufferNetworkModel : public NetworkModel
{
public:
   FiniteBufferNetworkModel(Network* network, SInt32 network_id);
   ~FiniteBufferNetworkModel();

   // NET_PACKET_INJECTOR
   static const SInt32 NET_PACKET_INJECTOR = 0;
   
   // Virtual Functions which are pure in network_model.h
   void reset() { }
   
   // Send Network Packet
   void sendNetPacket(NetPacket* raw_packet, list<NetPacket*>& modeling_packet_list_to_send);
   // Receive Network Packet
   void receiveNetPacket(NetPacket* net_packet, list<NetPacket*>& modeling_packet_list_to_send,
         list<NetPacket*>& raw_packet_list_to_receive);

   // Register/Unregister NetPacketInjectorExitCallback
   typedef void (*NetPacketInjectorExitCallback)(void*, UInt64);
   void registerNetPacketInjectorExitCallback(NetPacketInjectorExitCallback callback, void* obj);
   void unregisterNetPacketInjectorExitCallback();

   // Get NetworkNode
   NetworkNode* getNetworkNode(SInt32 node_index)
   { return _network_node_map[node_index]; }
   void setNetworkNode(SInt32 node_index, NetworkNode* network_node)
   { _network_node_map[node_index] = network_node; }

protected:
   // Network Nodes that are present on this core: There is a one-to-one mapping here
   map<SInt32, NetworkNode*> _network_node_map;
   // Flow Control Scheme
   FlowControlScheme::Type _flow_control_scheme;
   // Flow Control Packet Type
   PacketType _flow_control_packet_type;
   // CORE_INTERFACE port
   static const SInt32 CORE_INTERFACE = -1;

   // Create NetPacket Injector Node
   NetworkNode* createNetPacketInjectorNode(Router::Id ingress_router_id,
         BufferManagementScheme::Type ingress_router_buffer_management_scheme,
         SInt32 ingress_router_buffer_size);

   void outputContentionDelaySummary(ostream& out);

private:
   // Typedefs
   typedef map<UInt64,NetPacket*> PacketMap;
   typedef map<UInt64,UInt32> SequenceNumMap;
   
   class CompletePacket
   {
   public:
      CompletePacket(NetPacket* raw_packet, SInt32 zero_load_delay, UInt32 recv_sequence_num)
         : _raw_packet(raw_packet), _zero_load_delay(zero_load_delay), _recv_sequence_num(recv_sequence_num) {}
      ~CompletePacket() {}

      NetPacket* _raw_packet;
      SInt32 _zero_load_delay;
      UInt32 _recv_sequence_num;
   };
   typedef list<CompletePacket> CompletePacketList;
   
   // Sequence Numbers
   UInt32 _sender_sequence_num;
   
   // Maps to store modeling and raw packets
   PacketMap _recvd_raw_packet_map;
   PacketMap _recvd_modeling_packet_map;
  
   // Sequence Numbers to order packets at receiver
   vector<CompletePacketList> _vec_complete_packet_list;
   vector<UInt32> _vec_next_recv_sequence_num_to_be_assigned;
   vector<UInt32> _vec_next_recv_sequence_num_to_be_processed;
   SequenceNumMap _recv_sequence_num_map;
   
   // Sender Contention Model
   QueueModelSimple* _sender_contention_model;
   
   // Callback when packet leaves net packet injector
   NetPacketInjectorExitCallback _netPacketInjectorExitCallback;
   void* _netPacketInjectorExitCallbackObj;

   // Compute the output endpoints->[channel, index] of a particular flit
   virtual void computeOutputEndpointList(HeadFlit* head_flit, NetworkNode* curr_network_node) = 0;

   // Receive raw packet containing actual application data (non-modeling packet)
   void receiveRawPacket(NetPacket* raw_packet, list<NetPacket*>& raw_packet_list_to_receive);
   // Receive modeling packet containing timing information (non-raw packet)
   void receiveModelingPacket(NetPacket* modeling_packet, list<NetPacket*>& raw_packet_list_to_receive);

   // Insert a raw_packet in completed packets list
   void insertInCompletePacketList(NetPacket* raw_packet, SInt32 zero_load_delay);
   // Get the ready packets
   void getReadyPackets(SInt32 sender, list<NetPacket*>& raw_packet_list_to_receive);

   // Utils
   UInt64 computePacketId(core_id_t sender, UInt64 sequence_num);

   // Signal Injector that packet has left the network interface
   UInt64 getNetPacketInjectorExitTime(const list<NetPacket*>& modeling_packet_list);
   void signalNetPacketInjector(UInt64 time);

   // misc
   void printNetPacketList(const list<NetPacket*>& net_packet_list) const;
};
