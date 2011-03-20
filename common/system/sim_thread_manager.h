#ifndef SIM_THREAD_MANAGER_H
#define SIM_THREAD_MANAGER_H

#include "lock.h"
#include "sim_thread.h"

class SimThreadManager
{
public:
   SimThreadManager();
   ~SimThreadManager();

   void spawnThreads();
   void quitThreads();

   SInt32 registerThread();
   void unregisterThread();

private:
   // Core Id to Sim Thread Id mapping
   map<core_id_t, SInt32> _core_id__to__sim_thread_id__mapping;
   vector<vector<core_id_t> > _sim_thread_id__to__core_id_list__mapping;
   
   SimThread* m_sim_threads;

   Lock m_active_threads_lock;
   UInt32 m_active_threads;
};

#endif // SIM_THREAD_MANAGER
