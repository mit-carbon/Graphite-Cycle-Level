#pragma once

#include "network.h"
#include "network_model.h"
#include "router_power_model.h"
#include "electrical_link_performance_model.h"
#include "electrical_link_power_model.h"
#include "queue_model_simple.h"

class NetworkModelEMeshHopCounter : public NetworkModel
{
public:
   enum ModuleID
   {
      SENDER_CORE = 0,
      SENDER_ROUTER,
      RECEIVER_ROUTER,
      RECEIVER_CORE
   };

   NetworkModelEMeshHopCounter(Network *net, SInt32 network_id);
   ~NetworkModelEMeshHopCounter();

   volatile float getFrequency() { return _frequency; }
   
   UInt32 computeAction(const NetPacket& pkt);
   void routePacket(const NetPacket &pkt,
                    std::vector<Hop> &nextHops);
   void processReceivedPacket(const NetPacket* packet);

   void outputSummary(std::ostream &out);

   void reset() {}

private:
   volatile float _frequency;

   // Topology Parameters
   SInt32 _mesh_width;
   SInt32 _mesh_height;
   
   UInt64 _hop_latency;
   UInt32 _num_router_ports;
   
   // Router & Link Models
   RouterPowerModel* _electrical_router_power_model;
   ElectricalLinkPerformanceModel* _electrical_link_performance_model;
   ElectricalLinkPowerModel* _electrical_link_power_model;

   // Sender & Receiver Contention Models
   QueueModelSimple* _sender_contention_model;
   QueueModelSimple* _receiver_contention_model;

   // Event Counters
   UInt64 _total_switch_allocator_requests;
   UInt64 _total_crossbar_traversals;
   UInt64 _total_link_traversals;

   // Private Functions
   void computePosition(core_id_t core, SInt32 &x, SInt32 &y);
   SInt32 computeDistance(core_id_t sender, core_id_t receiver);
   UInt64 computeLatency(core_id_t sender, core_id_t receiver);

   void initializeEventCounters();

   // Power/Energy related
   void createRouterAndLinkModels();
   void destroyRouterAndLinkModels();

   void updateDynamicEnergy(const NetPacket& pkt, UInt32 contention, UInt32 num_hops);
   void outputPowerSummary(std::ostream& out); 
};
