#include <fstream>

#include "simulator.h"
#include "log.h"
#include "core.h"
#include "event_manager.h"
#include "core_manager.h"
#include "thread_manager.h"
#include "sim_thread_manager.h"
#include "sync_manager.h"
#include "syscall_manager.h"
#include "routine_manager.h"
#include "thread_interface.h"
#include "contrib/orion/orion.h"

Simulator *Simulator::m_singleton;
config::Config *Simulator::m_config_file;

static UInt64 getTime()
{
   timeval t;
   gettimeofday(&t, NULL);
   UInt64 time = (((UInt64)t.tv_sec) * 1000000 + t.tv_usec);
   return time;
}

void
Simulator::allocate()
{
   assert(m_singleton == NULL);
   m_singleton = new Simulator();
}

config::Config* Simulator::getConfigFile()
{
   return m_config_file;
}

void
Simulator::setConfigFile(config::Config *cfg)
{
   m_config_file = cfg;
}

void
Simulator::release()
{
   delete m_singleton;
   m_singleton = NULL;
}

Simulator*
Simulator::getSingleton()
{
   return m_singleton;
}

Simulator::Simulator()
   : m_config()
   , m_log(m_config)
   , m_event_manager(NULL)
   , m_core_manager(NULL)
   , m_thread_manager(NULL)
   , m_sim_thread_manager(NULL)
   , m_sync_manager(NULL)
   , m_syscall_manager(NULL)
   , m_boot_time(getTime())
   , m_start_time(0)
   , m_stop_time(0)
   , m_shutdown_time(0)
{
}

void
Simulator::start()
{
   LOG_PRINT("In Simulator ctor.");

   // Get Graphite Home
   char* graphite_home_str = getenv("GRAPHITE_HOME");
   _graphite_home = (graphite_home_str) ? ((string)graphite_home_str) : ".";
   
   // Create Orion Config Object
   string orion_cfg_file = _graphite_home + "/contrib/orion/orion.cfg";
   OrionConfig::allocate(orion_cfg_file);
   LOG_PRINT("Allocated OrionConfig");
   // OrionConfig::getSingleton()->print_config(cout);
 
   m_event_manager = new EventManager();
   LOG_PRINT("Created m_event_manager");
   m_core_manager = new CoreManager();
   LOG_PRINT("Created m_core_manager");
   m_thread_manager = new ThreadManager(m_core_manager);
   LOG_PRINT("Created m_thread_manager");
   m_sim_thread_manager = new SimThreadManager();
   LOG_PRINT("Created m_sim_thread_manager");
   m_sync_manager = new SyncManager();
   LOG_PRINT("Created m_sync_manager");
   m_syscall_manager = new SyscallManager();
   LOG_PRINT("Created m_syscall_manager");

   // App-Sim Thread Interfaces
   m_thread_interface_list.resize(Config::getSingleton()->getTotalCores());
   for (UInt32 i = 0; i < Config::getSingleton()->getTotalCores(); i++)
   {
      assert(m_core_manager);
      Core* core = m_core_manager->getCoreFromID(i);
      m_thread_interface_list[i] = new ThreadInterface(core);
   }
   LOG_PRINT("Created m_thread_interface_list");

   // Spawn Sim Threads
   m_sim_thread_manager->spawnSimThreads();
   LOG_PRINT("Spawned Sim Threads");

   Instruction::initializeStaticInstructionModel();
   LOG_PRINT("Initialized Static Instruction Model");

   LOG_PRINT("Simulator ctor exit");
}

Simulator::~Simulator()
{
   m_shutdown_time = getTime();

   LOG_PRINT("Simulator dtor starting...");

   m_sim_thread_manager->quitSimThreads();
   LOG_PRINT("Quit Sim Threads");

   // Core Summary
   ofstream os(Config::getSingleton()->getOutputFileName().c_str());

   os << "Simulation timers: " << endl
      << "start time\t" << (m_start_time - m_boot_time) << endl
      << "stop time\t" << (m_stop_time - m_boot_time) << endl
      << "shutdown time\t" << (m_shutdown_time - m_boot_time) << endl;

   m_core_manager->outputSummary(os);
   os.close();

   for (UInt32 i = 0; i < Config::getSingleton()->getTotalCores(); i++)
      delete m_thread_interface_list[i];
   m_thread_interface_list.clear();
   LOG_PRINT("Deleted Thread Interfaces");

   delete m_syscall_manager;
   LOG_PRINT("Deleted syscall_manager");
   delete m_sync_manager;
   LOG_PRINT("Deleted sync_manager");
   delete m_sim_thread_manager;
   LOG_PRINT("Deleted sim_thread_manager");
   delete m_thread_manager;
   LOG_PRINT("Deleted thread_manager");
   delete m_core_manager;
   m_core_manager = NULL;
   LOG_PRINT("Deleted core_manager");
   delete m_event_manager;
   LOG_PRINT("Deleted event_manager");

   // Delete Orion Config Object
   OrionConfig::release();
   LOG_PRINT("Released OrionConfig");
}

ThreadInterface*
Simulator::getThreadInterface(core_id_t core_id)
{ 
   assert(core_id >= 0 && core_id < (core_id_t) Config::getSingleton()->getTotalCores()); 
   return m_thread_interface_list[core_id];
}

void
Simulator::startTimer()
{
   m_start_time = getTime();
}

void
Simulator::stopTimer()
{
   m_stop_time = getTime();
}

void
Simulator::enablePerformanceModels()
{
   LOG_PRINT("Simulator::enablePerformanceModels()");
   emulateRoutine(Routine::ENABLE_PERFORMANCE_MODELS);
}

void
Simulator::disablePerformanceModels()
{
   LOG_PRINT("Simulator::disablePerformanceModels()");
   emulateRoutine(Routine::DISABLE_PERFORMANCE_MODELS);
}

void
Simulator::__enablePerformanceModels()
{
   LOG_PRINT("Simulator::enablePerformanceModels start");
   Sim()->startTimer();
   for (UInt32 i = 0; i < Config::getSingleton()->getTotalCores(); i++)
      Sim()->getCoreManager()->getCoreFromID(i)->enablePerformanceModels();
   LOG_PRINT("Simulator::enablePerformanceModels end");
}

void
Simulator::__disablePerformanceModels()
{
   LOG_PRINT("Simulator::disablePerformanceModels start");
   Sim()->stopTimer();
   for (UInt32 i = 0; i < Config::getSingleton()->getTotalCores(); i++)
      Sim()->getCoreManager()->getCoreFromID(i)->disablePerformanceModels();
   LOG_PRINT("Simulator::disablePerformanceModels end");
}
