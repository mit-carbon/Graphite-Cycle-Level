#pragma once

class NetPacket;
class Network;

#include <string>
#include <vector>

#include "config.h"
#include "packet_type.h"
#include "fixed_types.h"

// -- Network Models -- //

// To implement a new network model, you must implement this routing
// object. To route, take a packet and compute the next hop(s) and the
// time stamp for when that packet will be forwarded.
//   This lets one implement "magic" networks, analytical models,
// realistic hop-by-hop modeling, as well as broadcast models, such as
// a bus or ATAC.  Each static network has its own model object. This
// lets the user network be modeled accurately, while the MCP is a
// stupid magic network.
//   A packet will be dropped if no hops are filled in the nextHops
// vector.
class NetworkModel
{
public:
   NetworkModel(Network *network, SInt32 network_id, bool is_finite_buffer = false);
   virtual ~NetworkModel() { }

   class Hop
   {
   public:
      Hop(): 
         final_dest(INVALID_CORE_ID), 
         next_dest(INVALID_CORE_ID), 
         specific(0), 
         time(0) 
      {}
      ~Hop() {}

      // Final & Next destinations of a packet
      // 'final_dest' field is used to fill in the 'receiver' field in NetPacket
      SInt32 final_dest;
      SInt32 next_dest;

      // This field may be used by network models to fill in the 'specific' field in NetPacket
      // In general, specific field can be used by different network models for different functions
      UInt32 specific;
      
      // This field fills in the 'time' field in NetPacket
      UInt64 time;
   };

   class RoutingAction
   {
   public:
      enum type_t
      {
         RECEIVE = 0x001,
         FORWARD = 0x010,
         DROP = 0x100
      };
   };

   virtual volatile float getFrequency() = 0;

   virtual UInt32 computeAction(const NetPacket& packet) { assert(false); return 0; }
   virtual void routePacket(const NetPacket &packet, std::vector<Hop> &nextHops) { assert(false); }
   virtual void processReceivedPacket(const NetPacket* packet) {}

   virtual void outputSummary(std::ostream &out);

   void enable()  { _enabled = true; }
   void disable() { _enabled = false; }
   virtual void reset() = 0;

   SInt32 getNetworkId() { return _network_id; }
   std::string getNetworkName() { return _network_name; }

   // Get Serialization Latency
   SInt32 computeSerializationLatency(const NetPacket* raw_packet);
   // Is Packet Modeled
   bool isModeled(const NetPacket* packet);

   // Update Statistics on Packet Send
   void updatePacketSendStatistics(const NetPacket* packet);
   
   // Is Finite Buffer Network Model
   bool isFiniteBuffer() { return _is_finite_buffer; }
   static NetworkModel *createModel(Network* network, SInt32 network_id, UInt32 model_type);
   static UInt32 parseNetworkType(std::string str);

   static std::pair<bool,SInt32> computeCoreCountConstraints(UInt32 network_type, SInt32 core_count);
   static std::pair<bool, std::vector<core_id_t> > computeMemoryControllerPositions(UInt32 network_type, SInt32 num_memory_controllers);

protected:
   // If the network model is enabled
   bool _enabled;
   // Core Id on which this object is present
   core_id_t _core_id;
   // Flit Width
   SInt32 _flit_width;
   // Tile Width
   double _tile_width;

   static const SInt32 INFINITE_BANDWIDTH = -1;

   // Get pointer to network object
   Network *getNetwork() { return _network; }
  
   // Get Length of Packet sent in hardware
   SInt32 getModeledLength(const NetPacket* packet);
   // Get Requester for the packet
   core_id_t getRequester(const NetPacket* packet);

   // Update Statistics on Packet Receive
   void updatePacketReceiveStatistics(const NetPacket* pkt, SInt32 zero_load_delay);

private:
   Network *_network;
   
   SInt32 _network_id;
   std::string _network_name;

   bool _is_finite_buffer;
   
   // Performance Counters
   // Sent
   UInt64 _total_packets_sent;
   UInt64 _total_flits_sent;
   UInt64 _total_bytes_sent;
   // Broadcasted
   UInt64 _total_packets_broadcasted;
   UInt64 _total_flits_broadcasted;
   UInt64 _total_bytes_broadcasted;
   // Received
   UInt64 _total_packets_received;
   UInt64 _total_flits_received;
   UInt64 _total_bytes_received;
   // Delay Counters
   UInt64 _total_packet_latency;
   UInt64 _total_contention_delay;
   // Throughput Counters
   UInt64 _last_packet_send_time;
   UInt64 _last_packet_recv_time;
   
   // Initialization
   void initializePerformanceCounters();
};
