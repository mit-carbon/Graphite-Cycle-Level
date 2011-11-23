#pragma once

#include <vector>
using std::vector;
#include "config.h"
#include "config.hpp"
#include "log.h"
#include "thread_interface.h"

class EventManager;
class CoreManager;
class Thread;
class ThreadManager;
class SimThreadManager;
class SyncManager;
class SyscallManager;

class Simulator
{
public:
   Simulator();
   ~Simulator();

   void start();

   static Simulator* getSingleton();
   static config::Config* getConfigFile();
   static void setConfigFile(config::Config * cfg);
   static void allocate();
   static void release();

   EventManager* getEventManager() { return m_event_manager; }
   CoreManager *getCoreManager() { return m_core_manager; }
   ThreadManager *getThreadManager() { return m_thread_manager; }
   SimThreadManager *getSimThreadManager() { return m_sim_thread_manager; }
   SyncManager *getSyncManager() { return m_sync_manager; }
   SyscallManager *getSyscallManager() { return m_syscall_manager; }
   ThreadInterface *getThreadInterface(core_id_t core_id);
   Config *getConfig() { return &m_config; }
   config::Config *getCfg() { return m_config_file; }

   static void enablePerformanceModels();
   static void disablePerformanceModels();
   static void __enablePerformanceModels();
   static void __disablePerformanceModels();

   std::string getGraphiteHome() { return _graphite_home; }

private:

   Config m_config;
   Log m_log;
   EventManager *m_event_manager;
   CoreManager *m_core_manager;
   ThreadManager *m_thread_manager;
   SimThreadManager *m_sim_thread_manager;
   SyncManager *m_sync_manager;
   SyscallManager *m_syscall_manager;
   vector<ThreadInterface*> m_thread_interface_list;

   static Simulator *m_singleton;

   UInt64 m_boot_time;
   UInt64 m_shutdown_time;
   
   static config::Config *m_config_file;

   std::string _graphite_home;
};

__attribute__((unused)) static Simulator *Sim()
{
   return Simulator::getSingleton();
}
