#include <vector>

#include "core_manager.h"
#include "core.h"
#include "config.h"
#include "log.h"

using namespace std;

CoreManager::CoreManager()
      : m_core_tls(TLS::create())
      , m_thread_type_tls(TLS::create())
      , m_num_registered_sim_threads(0)
{
   LOG_PRINT("Starting CoreManager Constructor.");

   UInt32 num_cores = Config::getSingleton()->getTotalCores();
   for (UInt32 i = 0; i < num_cores; i++)
   {
      m_cores.push_back(new Core(i));
      m_initialized_cores.push_back(false);
   }

   LOG_PRINT("Finished CoreManager Constructor.");
}

CoreManager::~CoreManager()
{
   LOG_PRINT("Core Manager dtor start");
   for (std::vector<Core*>::iterator i = m_cores.begin(); i != m_cores.end(); i++)
   {
      delete *i;
   }
   LOG_PRINT("Deleted all cores");
   LOG_PRINT("Deleting (%p)", m_core_tls);
   delete m_core_tls;
   LOG_PRINT("Deleted m_core_tls");
   delete m_thread_type_tls;
   LOG_PRINT("Deleted m_thread_type_tls");
   LOG_PRINT("Core Manager dtor end");
}

void
CoreManager::initializeThread()
{
   LOG_PRINT("initializeThread() enter");

   ScopedLock sl(m_initialized_cores_lock);

   for (core_id_t i = 0; i < (core_id_t) m_initialized_cores.size(); i++)
   {
       if (!m_initialized_cores.at(i))
       {
           doInitializeThread(i);
           return;
       }
   }

   LOG_PRINT_ERROR("initializeThread - No free cores out of %d total.",
         Config::getSingleton()->getTotalCores());
}

void
CoreManager::initializeThread(core_id_t core_id)
{
   LOG_PRINT("initializeThread(%i) enter", core_id);

   ScopedLock sl(m_initialized_cores_lock);

   assert(core_id >= 0 && core_id < (core_id_t) Config::getSingleton()->getTotalCores());
   LOG_ASSERT_ERROR (!m_initialized_cores.at(core_id), 
         "initializeThread -- %d/%d already mapped",
         core_id, Config::getSingleton()->getTotalCores());

   doInitializeThread(core_id);
   
   LOG_PRINT("initializeThread(%i) exit", core_id);
}

void
CoreManager::doInitializeThread(core_id_t core_id)
{
   LOG_PRINT("doInitializeThread(%i) enter", core_id);
   
   m_core_tls->set(m_cores.at(core_id));
   m_thread_type_tls->setInt(APP_THREAD);
   m_initialized_cores.at(core_id) = true;
   LOG_ASSERT_ERROR(m_core_tls->get() == (void*)(m_cores.at(core_id)),
                     "TLS appears to be broken. %p != %p",
                     m_core_tls->get(), (void*)(m_cores.at(core_id)));
   
   LOG_PRINT("doInitializeThread(%i) exit", core_id);
}

void
CoreManager::terminateThread()
{
   Core* core = m_core_tls->getPtr<Core>();
   LOG_ASSERT_WARNING(core, "Thread not initialized while terminating.");

   m_initialized_cores.at(core->getId()) = false;
   m_core_tls->set(NULL);
}

Core*
CoreManager::getCurrentCore()
{
    return m_core_tls->getPtr<Core>();
}

core_id_t
CoreManager::getCurrentCoreID()
{
   Core *core = getCurrentCore();
   if (!core)
       return INVALID_CORE_ID;
   else
       return core->getId();
}

Core*
CoreManager::getCoreFromID(core_id_t id)
{
   assert(id >= 0 && id < (core_id_t) Config::getSingleton()->getTotalCores());
   return m_cores.at(id);
}

core_id_t
CoreManager::registerSimThread(core_id_t core_id)
{
    if (getCurrentCore() != NULL)
    {
        LOG_PRINT_ERROR("registerSimMemThread - Initialized thread twice");
        return getCurrentCore()->getId();
    }

    ScopedLock sl(m_num_registered_sim_threads_lock);

    LOG_ASSERT_ERROR(m_num_registered_sim_threads < Config::getSingleton()->getTotalSimThreads(),
                     "All sim threads already registered. %d > %d",
                     m_num_registered_sim_threads+1, Config::getSingleton()->getTotalSimThreads());

    Core* core = m_cores.at(core_id);

    m_core_tls->set(core);
    m_thread_type_tls->setInt(SIM_THREAD);

    ++m_num_registered_sim_threads;

    return core->getId();
}

bool
CoreManager::amiAppThread()
{
    return m_thread_type_tls->getInt() == APP_THREAD;
}

bool
CoreManager::amiSimThread()
{
    return m_thread_type_tls->getInt() == SIM_THREAD;
}
