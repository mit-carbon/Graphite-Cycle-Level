#ifndef __NETWORK_MODEL_EMESH_HOP_BY_HOP_GENERIC_H__
#define __NETWORK_MODEL_EMESH_HOP_BY_HOP_GENERIC_H__

#include "network.h"
#include "network_model.h"
#include "fixed_types.h"
#include "queue_model.h"
#include "lock.h"
#include "router_power_model.h"
#include "electrical_link_performance_model.h"
#include "electrical_link_power_model.h"

class NetworkModelEMeshHopByHopGeneric : public NetworkModel
{
   public:
      typedef enum
      {
         UP = 0,
         DOWN,
         LEFT,
         RIGHT,
         NUM_OUTPUT_DIRECTIONS,
         SELF  // Does not have a queue corresponding to this
      } OutputDirection;

   private:
      // Fields
      core_id_t m_core_id;
      static SInt32 m_mesh_width;
      static SInt32 m_mesh_height;

      // Router & Link Parameters
      UInt32 m_num_router_ports;
      UInt64 m_hop_latency;

      // Router & Link models
      RouterPowerModel* m_electrical_router_power_model;
      ElectricalLinkPerformanceModel* m_electrical_link_performance_model;
      ElectricalLinkPowerModel* m_electrical_link_power_model;

      QueueModel* m_queue_models[NUM_OUTPUT_DIRECTIONS];
      QueueModel* m_injection_port_queue_model;
      QueueModel* m_ejection_port_queue_model;

      bool m_enabled;

      // Lock
      Lock m_lock;

      // Performance Counters
      UInt64 m_total_bytes_received;
      UInt64 m_total_packets_received;
      UInt64 m_total_contention_delay;
      UInt64 m_total_packet_latency;

      // Activity Counters
      UInt64 m_switch_allocator_traversals;
      UInt64 m_crossbar_traversals;
      UInt64 m_buffer_accesses;
      UInt64 m_link_traversals;

      // Functions
      void computePosition(core_id_t core, SInt32 &x, SInt32 &y);
      core_id_t computeCoreId(SInt32 x, SInt32 y);
      SInt32 computeDistance(core_id_t sender, core_id_t receiver);

      void addHop(OutputDirection direction, core_id_t final_dest, core_id_t next_dest, \
            const NetPacket& pkt, UInt64 pkt_time, UInt32 pkt_length, \
            std::vector<Hop>& nextHops, \
            core_id_t requester);
      UInt64 computeLatency(OutputDirection direction, \
            const NetPacket& pkt, UInt64 pkt_time, UInt32 pkt_length, \
            core_id_t requester);
      UInt64 computeProcessingTime(UInt32 pkt_length);
      core_id_t getNextDest(core_id_t final_dest, OutputDirection& direction);
      core_id_t getRequester(const NetPacket& pkt);

      // Injection & Ejection Port Queue Models
      UInt64 computeInjectionPortQueueDelay(core_id_t pkt_receiver, UInt64 pkt_time, UInt32 pkt_length);
      UInt64 computeEjectionPortQueueDelay(const NetPacket& pkt, UInt64 pkt_time, UInt32 pkt_length);

      static void initializeEMeshTopologyParams();
      void createQueueModels();
      void destroyQueueModels();
      void resetQueueModels();

      // Router & Link Models
      void createRouterAndLinkModels();
      void destroyRouterAndLinkModels();

      // Performance Counters
      void initializePerformanceCounters();
      // Activity Counters for Power
      void initializeActivityCounters();
      
      // Update Dynamic Energy
      void updateDynamicEnergy(const NetPacket& pkt, bool is_buffered, UInt32 contention);
      void outputPowerSummary(std::ostream& out);

   protected:
      volatile float m_frequency;

      // Link Characteristics
      UInt32 m_link_width;
      volatile double m_link_length;
      std::string m_link_type;

      // Router Characteristics
      UInt64 m_router_delay;
      UInt32 m_num_flits_per_output_buffer;

      // Check if the electrical mesh has a broadcast tree
      bool m_broadcast_tree_enabled;
      
      bool m_queue_model_enabled;
      std::string m_queue_model_type;

      void initializeModels();
   
   public:
      NetworkModelEMeshHopByHopGeneric(Network* net, SInt32 network_id);
      ~NetworkModelEMeshHopByHopGeneric();

      volatile float getFrequency() { return m_frequency; }

      UInt32 computeAction(const NetPacket& pkt);
      void routePacket(const NetPacket &pkt, std::vector<Hop> &nextHops);
      void processReceivedPacket(NetPacket &pkt);

      static std::pair<bool,std::vector<core_id_t> > computeMemoryControllerPositions(SInt32 num_memory_controllers);
      static std::pair<bool,SInt32> computeCoreCountConstraints(SInt32 core_count);
      static std::pair<bool,std::vector<Config::CoreList> > computeProcessToCoreMapping();

      static SInt32 computeNumHops(core_id_t sender, core_id_t receiver);

      void outputSummary(std::ostream &out);

      void enable();
      void disable();
      void reset();
};

#endif /* __NETWORK_MODEL_EMESH_HOP_BY_HOP_GENERIC_H__ */
