#ifndef __SHMEM_PERF_MODEL_H__
#define __SHMEM_PERF_MODEL_H__

#include <cassert>
#include <iostream>

#include "lock.h"

using namespace std;

class ShmemPerfModel
{
   public:
      enum Thread_t
      {
         _APP_THREAD = 0,
         _SIM_THREAD,
         NUM_CORE_THREADS
      };

   private:
      UInt64 m_cycle_count[NUM_CORE_THREADS];
      bool m_enabled;
      Lock m_shmem_perf_model_lock;

      UInt64 m_num_memory_accesses;
      UInt64 m_total_memory_access_latency_in_clock_cycles;
      UInt64 m_total_memory_access_latency_in_ns;

      Thread_t getThreadNum();
      void initializePerformanceCounters();

   public:
      ShmemPerfModel();
      ~ShmemPerfModel();

      void setCycleCount(Thread_t thread_num, UInt64 count);

      void setCycleCount(UInt64 count)
      {
         setCycleCount(getThreadNum(), count);
      }
      UInt64 getCycleCount();
      void incrCycleCount(UInt64 count);
      void updateCycleCount(UInt64 count);

      void incrTotalMemoryAccessLatency(UInt64 shmem_time);

      void updateInternalVariablesOnFrequencyChange(volatile float core_frequency);
      
      void enable();
      void disable();
      void reset();
      bool isEnabled() { return m_enabled; }

      void outputSummary(ostream& out, volatile float core_frequency);
};

#endif /* __SHMEM_PERF_MODEL_H__ */
