#include "sim_thread_manager.h"

#include "lock.h"
#include "log.h"
#include "config.h"
#include "simulator.h"
#include "transport.h"

SimThreadManager::SimThreadManager()
   : m_active_threads(0)
{
}

SimThreadManager::~SimThreadManager()
{
   LOG_ASSERT_WARNING(m_active_threads == 0,
                      "Threads still active when SimThreadManager exits.");
}

void SimThreadManager::spawnSimThreads()
{
   m_simulation_running = true;
   UInt32 num_sim_threads = Config::getSingleton()->getTotalSimThreads();

   LOG_PRINT("Starting %d threads on proc: %d.", num_sim_threads, Config::getSingleton()->getCurrentProcessNum());

   m_sim_threads = new SimThread[num_sim_threads];

   for (UInt32 i = 0; i < num_sim_threads; i++)
   {
      LOG_PRINT("Starting thread %i", i);
      m_sim_threads[i].spawn();
   }

   while (m_active_threads < num_sim_threads)
      sched_yield();

   LOG_PRINT("Threads started: %d.", m_active_threads);
}

void SimThreadManager::quitSimThreads()
{
   m_simulation_running = false; 
   LOG_PRINT("Waiting for local sim threads to exit.");

   while (m_active_threads > 0)
      sched_yield();

   Transport::getSingleton()->barrier();

   LOG_PRINT("All threads have exited.");
}

SInt32 SimThreadManager::registerSimThread()
{
   m_active_threads_lock.acquire();
   SInt32 sim_thread_id = m_active_threads++;
   m_active_threads_lock.release();

   return sim_thread_id;
}

void SimThreadManager::unregisterSimThread()
{
   m_active_threads_lock.acquire();
   m_active_threads--;
   m_active_threads_lock.release();
}
