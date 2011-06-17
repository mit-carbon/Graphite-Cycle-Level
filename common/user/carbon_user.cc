#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include "simulator.h"
#include "thread_manager.h"
#include "core_manager.h"
#include "core.h"
#include "clock_converter.h"
#include "config_file.hpp"
#include "handle_args.h"
#include "routine_manager.h"

#include "carbon_user.h"
#include "thread_support_private.h"

static config::ConfigFile cfg;

int CarbonStartSim(int argc, char **argv)
{
   string_vec args;

   // Set the default config path if it isn't 
   // overwritten on the command line.
   std::string config_path = "carbon_sim.cfg";

   // Parse the arguments that are relative
   // to the config file changes as well
   // as extracting the config_path
   parse_args(args, config_path, argc, argv);

   cfg.load(config_path);
   handle_args(args, cfg);

   Simulator::setConfigFile(&cfg);

   Simulator::allocate();
   Sim()->start();

   // Set Pin Mode
   Config::getSingleton()->setExecutionMode(Config::NATIVE);

   // Main process
   Sim()->getThreadManager()->onThreadStart(0);

   LOG_PRINT("Returning to main()...");
   return 0;
}

void CarbonStopSim()
{
   // Main Thread
   Sim()->getThreadManager()->onThreadExit();

   Simulator::release();
}

UInt64 CarbonGetTime()
{
   return emulateRoutine(Routine::CARBON_GET_TIME);
}

UInt64 __CarbonGetTime(core_id_t core_id)
{
   Core* core = Sim()->getCoreManager()->getCoreFromID(core_id);
   return core->getPerformanceModel()->getTime();
}
