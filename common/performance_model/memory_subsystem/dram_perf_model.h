#pragma once

#include <iostream>

#include "queue_model_simple.h"
#include "fixed_types.h"

// Note: Each Dram Controller owns a single DramModel object
// Hence, m_dram_bandwidth is the bandwidth for a single DRAM controller
// Total Bandwidth = m_dram_bandwidth * Number of DRAM controllers
// Number of DRAM controllers presently = Number of Cores
// m_dram_bandwidth is expressed in GB/s
// Assuming the frequency of a core is 1GHz, 
// m_dram_bandwidth is also expressed in 'Bytes per clock cycle'
// This DRAM model is not entirely correct.
// It sort of increases the queueing delay to a huge value if
// the arrival times of adjacent packets are spread over a large
// simulated time period
class DramPerfModel
{
   private:
      // Dram Model Parameters
      UInt64 m_dram_access_cost;
      volatile float m_dram_bandwidth;

      // Queue Model
      QueueModelSimple* m_queue_model;
      bool m_queue_model_enabled;
      
      bool m_enabled;

      // Performance Counters
      UInt64 m_num_accesses;
      volatile double m_total_access_latency;
      volatile double m_total_queueing_delay;

      void initializePerformanceCounters();

   public:
      DramPerfModel(float dram_access_cost, 
            float dram_bandwidth,
            bool queue_model_enabled);
      ~DramPerfModel();

      UInt64 getAccessLatency(UInt64 pkt_time, UInt64 pkt_size, core_id_t requester);
      void enable()
      { m_enabled = true; }
      void disable()
      { m_enabled = false; }
      void reset() {}

      UInt64 getTotalAccesses() { return m_num_accesses; }
      void outputSummary(std::ostream& out);

      static void dummyOutputSummary(std::ostream& out);
};
