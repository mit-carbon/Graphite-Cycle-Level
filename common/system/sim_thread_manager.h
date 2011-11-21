#ifndef SIM_THREAD_MANAGER_H
#define SIM_THREAD_MANAGER_H

#include <vector>
using std::vector;

#include "lock.h"
#include "sim_thread.h"

class SimThreadManager
{
public:
   SimThreadManager();
   ~SimThreadManager();

   void spawnSimThreads();
   void quitSimThreads();

   SInt32 registerThread();
   void unregisterThread();

   vector<core_id_t>& getCoreIDListFromSimThreadID(SInt32 sim_thread_id);
   SInt32 getSimThreadIDFromCoreID(core_id_t core_id);
   
private:
   // Core Id to Sim Thread Id mapping
   vector<SInt32> _core_id__to__sim_thread_id__mapping;
   vector<vector<core_id_t> > _sim_thread_id__to__core_id_list__mapping;
   
   SimThread* m_sim_threads;

   Lock m_active_threads_lock;
   UInt32 m_active_threads;

   static const UInt32 TERMINATE_SIM_THREAD = 1000;

   // Initialization of core id --> sim thread id mapping
   void initializeSimThreadIDToCoreIDMappings();
};

#endif // SIM_THREAD_MANAGER
