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

   void enable() { _enabled = true; }
   void disable() { _enabled = false; }

   // Virtual Functions which are pure in network_model.h
   void reset() { }
   UInt32 computeAction(const NetPacket& pkt) { assert(false); return 0; }
   void routePacket(const NetPacket& pkt, vector<Hop>& nextHops) { assert(false); }
   void processReceivedPacket(NetPacket& pkt) { assert(false); }
   
   // Output Summary
   void outputSummary(ostream& out);

   // Send Network Packet
   void sendNetPacket(NetPacket* raw_packet, list<NetPacket*>& modeling_packet_list_to_send);
   // Receive Network Packet
   void receiveNetPacket(NetPacket* net_packet, list<NetPacket*>& modeling_packet_list_to_send,
         list<NetPacket*>& raw_packet_list_to_receive);

   // Get NetworkNode
   NetworkNode* getNetworkNode(SInt32 node_index)
   { return _network_node_map[node_index]; }

protected:
   // If the network model is enabled
   bool _enabled;
   // Core Id on which this object is present
   core_id_t _core_id;
   // Network Nodes that are present on this core: There is a one-to-one mapping here
   map<SInt32, NetworkNode*> _network_node_map;
   // Flow Control Scheme
   FlowControlScheme::Type _flow_control_scheme;
   // Flow Control Packet Type
   PacketType _flow_control_packet_type;
   // Flit Width
   SInt32 _flit_width;
   // CORE_INTERFACE port
   static const SInt32 CORE_INTERFACE = -1;

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
   
   // Lock
   Lock _lock;
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
   
   // Performance Counters
   UInt64 _total_packets_received;
   UInt64 _total_bytes_received;
   UInt64 _total_packet_latency;
   UInt64 _total_contention_delay;

   // Compute the output endpoints->[channel, index] of a particular flit
   virtual void computeOutputEndpointList(HeadFlit* head_flit, NetworkNode* curr_network_node) = 0;
   // Compute the ingress router id
   virtual Router::Id computeIngressRouterId(core_id_t core_id) = 0;

   // Process Received Packet to record packet delays
   void updatePacketStatistics(const NetPacket* pkt, SInt32 zero_load_delay);
   
   // Receive raw packet containing actual application data (non-modeling packet)
   void receiveRawPacket(NetPacket* raw_packet, list<NetPacket*>& raw_packet_list_to_receive);
   // Receive modeling packet containing timing information (non-raw packet)
   void receiveModelingPacket(NetPacket* modeling_packet, list<NetPacket*>& raw_packet_list_to_receive);

   // Insert a raw_packet in completed packets list
   void insertInCompletePacketList(NetPacket* raw_packet, SInt32 zero_load_delay);
   // Get the ready packets
   void getReadyPackets(SInt32 sender, list<NetPacket*>& raw_packet_list_to_receive);

   // Utils
   SInt32 computeSerializationLatency(const NetPacket* raw_packet);
   UInt64 computePacketId(core_id_t sender, UInt64 sequence_num);

   // Initialization
   void initializePerformanceCounters();

   // misc
   void printNetPacketList(const list<NetPacket*>& net_packet_list) const;
};
