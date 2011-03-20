#include "sim_thread_manager.h"

#include "lock.h"
#include "log.h"
#include "config.h"
#include "simulator.h"
#include "transport.h"

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
   SInt32 num_sim_threads = Config::getSingleton()->getLocalSimThreadCount();
   // Compute a stupid mapping -- Refine Later
   SInt32 sim_thread_id = 0;
   vector<core_id_t>& core_id_list = Config::getSingleton()->getCoreListForCurrentProcess();
   for (vector<core_id_t>::iterator i = core_id_list.begin(); i != core_id_list.end(); i++)
   {
      _core_id__to__sim_thread_id__mapping[*i] = sim_thread_id;
      _sim_thread_id__to__core_id_list__mapping[sim_thread_id].push_back(*i);
      sim_thread_id = (sim_thread_id + 1) % num_sim_threads;
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
SimThreadManager::spawnThreads(SimThread::Type sim_thread_type)
{
   UInt32 num_sim_threads = Config::getSingleton()->getLocalSimThreadCount();

   LOG_PRINT("Starting %d threads on proc: %d.", 
         num_sim_threads, Config::getSingleton()->getCurrentProcessNum());

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
SimThreadManager::quitThreads()
{
   LOG_PRINT("Sending quit messages.");

   Transport::Node *global_node = Transport::getSingleton()->getGlobalNode();
   UInt32 num_sim_threads = Config::getSingleton()->getLocalSimThreadCount();

   // This is something of a hard-wired emulation of Network::netSend
   // ... not the greatest thing to do, but whatever.
   NetPacket pkt = new NetPacket(0, SIM_THREAD_TERMINATE_THREADS, 0, 0, 0, NULL);

   for (UInt32 i = 0; i < num_sim_threads; i++)
   {
      core_id_t core_id = _sim_thread_id__to__core_id_list__mapping[i].front();
      pkt.receiver = core_id;

      EventNetwork* event = new EventNetwork(0 /* time */,
                                             pkt.receiver, pkt, 
                                             global_node)
      Sim()->getEventManager()->processEventInOrder(event,
            getSimThreadIDFromCoreID(pkt.receiver), EventQueue::UNORDERED);
   }

   LOG_PRINT("Waiting for local sim threads to exit.");

   while (m_active_threads > 0)
      sched_yield();

   Transport::getSingleton()->barrier();

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
