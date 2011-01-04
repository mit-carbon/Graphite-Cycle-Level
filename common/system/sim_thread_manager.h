#ifndef SIM_THREAD_MANAGER_H
#define SIM_THREAD_MANAGER_H

#include "lock.h"
#include "sim_thread.h"

class SimThreadManager
{
public:
   SimThreadManager();
   ~SimThreadManager();

   void spawnSimThreads();
   void quitSimThreads();

   SInt32 registerSimThread();
   void unregisterSimThread();

   bool isSimulationRunning()
   { return m_simulation_running; }
   
private:
   SimThread *m_sim_threads;

   Lock m_active_threads_lock;
   UInt32 m_active_threads;

   bool m_simulation_running;
};

#endif // SIM_THREAD_MANAGER
