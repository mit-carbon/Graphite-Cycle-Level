#include "sim_thread_manager.h"

#include "lock.h"
#include "log.h"
#include "config.h"
#include "simulator.h"
#include "event.h"

SimThreadManager::SimThreadManager()
   : m_active_threads(0)
{
   initializeSimThreadIDToCoreIDMappings();
}

SimThreadManager::~SimThreadManager()
{
   LOG_ASSERT_WARNING(m_active_threads == 0,
                      "Threads still active when SimThreadManager exits.");
}

// Sim Thread ID --> Core ID List (Mapping)
// Core ID --> Sim Thread ID (Mapping)
void
SimThreadManager::initializeSimThreadIDToCoreIDMappings()
{
   // Compute the mapping from core id to sim thread id
   SInt32 num_cores = Config::getSingleton()->getTotalCores();
   SInt32 num_sim_threads = Config::getSingleton()->getTotalSimThreads();
   // Compute a stupid mapping -- Refine Later
   SInt32 sim_thread_id = 0;
   
   SInt32 num_cores_per_sim_thread = num_cores / num_sim_threads;

   _sim_thread_id__to__core_id_list__mapping.resize(num_sim_threads);
   _core_id__to__sim_thread_id__mapping.resize(num_cores);

   for (core_id_t core_id = 0; core_id < num_cores; )
   {
      _core_id__to__sim_thread_id__mapping[core_id] = sim_thread_id;
      _sim_thread_id__to__core_id_list__mapping[sim_thread_id].push_back(core_id);
      
      core_id ++;      
      
      if ((core_id % num_cores_per_sim_thread) == 0)
         sim_thread_id = ((sim_thread_id + 1) % num_sim_threads);
      assert(sim_thread_id < num_sim_threads);
   }
}

vector<core_id_t>&
SimThreadManager::getCoreIDListFromSimThreadID(SInt32 sim_thread_id)
{
   return _sim_thread_id__to__core_id_list__mapping[sim_thread_id];
}

SInt32
SimThreadManager::getSimThreadIDFromCoreID(core_id_t core_id)
{
   return _core_id__to__sim_thread_id__mapping[core_id];
}

void
SimThreadManager::spawnSimThreads()
{
   // Register the exit handler
   Event::registerHandler(TERMINATE_SIM_THREAD, SimThread::handleTerminationRequest);

   UInt32 num_sim_threads = Config::getSingleton()->getTotalSimThreads();

   LOG_PRINT("Starting %d threads", num_sim_threads);
   
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

void
SimThreadManager::quitSimThreads()
{
   LOG_PRINT("Sending quit messages.");

   UInt32 num_sim_threads = Config::getSingleton()->getTotalSimThreads();

   for (UInt32 i = 0; i < num_sim_threads; i++)
   {
      // Get a single core id
      vector<core_id_t>& core_id_list = _sim_thread_id__to__core_id_list__mapping[i];
      core_id_t core_id = core_id_list.front();

      UnstructuredBuffer* event_args = new UnstructuredBuffer();
      (*event_args) << &m_sim_threads[i];
      Event* event = new Event((Event::Type) TERMINATE_SIM_THREAD, 0 /* time */, event_args);
      Event::processInOrder(event, core_id, EventQueue::UNORDERED);
   }

   LOG_PRINT("Waiting for local sim threads to exit.");

   while (m_active_threads > 0)
      sched_yield();

   delete [] m_sim_threads;

   // Unregister the sim thread exit handler
   Event::unregisterHandler(TERMINATE_SIM_THREAD);

   LOG_PRINT("All threads have exited.");
}

SInt32
SimThreadManager::registerThread()
{
   m_active_threads_lock.acquire();
   SInt32 sim_thread_id = m_active_threads++;
   m_active_threads_lock.release();

   return sim_thread_id;
}

void
SimThreadManager::unregisterThread()
{
   m_active_threads_lock.acquire();
   m_active_threads--;
   m_active_threads_lock.release();
}
