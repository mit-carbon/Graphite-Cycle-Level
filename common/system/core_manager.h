#pragma once

#include <iostream>
#include <vector>

#include "fixed_types.h"
#include "tls.h"
#include "lock.h"

class Core;

class CoreManager
{
   public:
      CoreManager();
      ~CoreManager();

      void initializeThread();
      void initializeThread(core_id_t core_id);
      void terminateThread();
      core_id_t registerSimThread(core_id_t core_id);

      core_id_t getCurrentCoreID(); // id of currently active core (or INVALID_CORE_ID)

      Core *getCurrentCore();
      Core *getCoreFromID(core_id_t id);

      void outputSummary(std::ostream &os);

      bool amiAppThread();
      bool amiSimThread();
   private:

      void doInitializeThread(core_id_t core_id);

      TLS *m_core_tls;
      TLS *m_thread_type_tls;

      enum ThreadType {
          INVALID,
          APP_THREAD,
          SIM_THREAD
      };

      std::vector<bool> m_initialized_cores;
      Lock m_initialized_cores_lock;

      UInt32 m_num_registered_sim_threads;
      Lock m_num_registered_sim_threads_lock;

      std::vector<Core*> m_cores;
};
