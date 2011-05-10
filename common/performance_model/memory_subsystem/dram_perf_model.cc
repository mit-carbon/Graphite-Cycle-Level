#include <iostream>
using namespace std;

#include "simulator.h"
#include "config.h"
#include "dram_perf_model.h"
#include "queue_model_history_list.h"
#include "queue_model_history_tree.h"

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
DramPerfModel::DramPerfModel(float dram_access_cost, 
      float dram_bandwidth,
      bool queue_model_enabled):
   m_dram_access_cost(UInt64(dram_access_cost)),
   m_dram_bandwidth(dram_bandwidth),
   m_queue_model(NULL),
   m_queue_model_enabled(queue_model_enabled),
   m_enabled(false)
{
   initializePerformanceCounters();
   
   if (m_queue_model_enabled)
      m_queue_model = new QueueModelSimple();
}

DramPerfModel::~DramPerfModel()
{
   if (m_queue_model_enabled)
      delete m_queue_model;
}

void
DramPerfModel::initializePerformanceCounters()
{
   m_num_accesses = 0;
   m_total_access_latency = 0;
   m_total_queueing_delay = 0;
}

UInt64 
DramPerfModel::getAccessLatency(UInt64 pkt_time, UInt64 pkt_size, core_id_t requester)
{
   // pkt_size is in 'Bytes'
   // m_dram_bandwidth is in 'Bytes per clock cycle'
   if (!m_enabled)
      return 0;

   assert(m_enabled);

   UInt64 processing_time = (UInt64) ((float) pkt_size/m_dram_bandwidth) + 1;

   // Compute Queue Delay
   UInt64 queue_delay;
   if (m_queue_model)
      queue_delay = m_queue_model->computeQueueDelay(pkt_time, processing_time, requester);
   else
      queue_delay = 0;

   UInt64 access_latency = queue_delay + processing_time + m_dram_access_cost;

   // Update Memory Counters
   m_num_accesses ++;
   m_total_access_latency += (double) access_latency;
   m_total_queueing_delay += (double) queue_delay;

   return access_latency;
}

void
DramPerfModel::outputSummary(ostream& out)
{
   out << "Dram Perf Model summary: " << endl;
   out << "    num dram accesses: " << m_num_accesses << endl;
   out << "    average dram access latency: " << 
      (float) (m_total_access_latency / m_num_accesses) << endl;
   out << "    average dram queueing delay: " << 
      (float) (m_total_queueing_delay / m_num_accesses) << endl;
}

void
DramPerfModel::dummyOutputSummary(ostream& out)
{
   out << "Dram Perf Model summary: " << endl;
   out << "    num dram accesses: NA" << endl;
   out << "    average dram access latency: NA" << endl;
   out << "    average dram queueing delay: NA" << endl;
}
