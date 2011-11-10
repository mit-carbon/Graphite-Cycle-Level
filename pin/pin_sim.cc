// Graphite Simulator
//
// This simulator models future multi-core computers with thousands of cores.
// It runs on today's x86 multicores and will scale as more and more cores
// and better inter-core communications mechanisms become available.
// The simulator provides a platform for research in processor architecture,
// compilers, network interconnect topologies, and some OS.
//
// The simulator runs on top of Intel's Pin dynamic binary instrumention engine.
// Application code in the absence of instrumentation runs more or less
// natively and is thus high performance. When instrumentation is used, models
// can be hot-swapped or dynamically enabled and disabled as desired so that
// performance tracks the level of simulation detail needed.

#include <iostream>

#include "pin.H"
#include "simulator.h"
#include "core_manager.h"
#include "config.h"
#include "core.h"
#include "thread_manager.h"
#include "config_file.hpp"
#include "handle_args.h"
#include "log.h"
#include "progress_trace.h"
#include "routine_replace.h"
#include "handle_syscalls.h"
#include "instruction_modeling.h"

bool done_app_initialization = false;
config::ConfigFile *cfg;

VOID instructionCallback (INS ins, void *v)
{
   // Core Performance Modeling
   if (Config::getSingleton()->getEnablePerformanceModeling())
      addInstructionModeling(ins);

   // Syscall Handling
   if (INS_IsSyscall(ins))
   {
      INS_InsertCall(ins, IPOINT_BEFORE,
            AFUNPTR(handleSyscall),
            IARG_CONTEXT,
            IARG_END);
   }
}

void ApplicationStart()
{
}

void ApplicationExit(int, void*)
{
   LOG_PRINT("Application exit.");
   Simulator::release();
   shutdownProgressTrace();
   delete cfg;
}

VOID threadStartCallback(THREADID threadIndex, CONTEXT *ctxt, INT32 flags, VOID *v)
{
   threadStartProgressTrace();

   if (! done_app_initialization) // Thread 0
   {
      Sim()->getThreadManager()->onThreadStart(0);
      done_app_initialization = true;
   }
   else // Other threads
   {
      ThreadSpawnRequest req = Sim()->getThreadManager()->dequeueThreadSpawnReq();
      Sim()->getThreadManager()->onThreadStart(req.core_id);
   }
}

VOID threadFiniCallback(THREADID threadIndex, const CONTEXT *ctxt, INT32 flags, VOID *v)
{
   Sim()->getThreadManager()->onThreadExit();
}

int main(int argc, char *argv[])
{
   // Global initialization
   PIN_InitSymbols();
   PIN_Init(argc,argv);

   string_vec args;

   // Set the default config path if it isn't 
   // overwritten on the command line.
   std::string config_path = "carbon_sim.cfg";

   parse_args(args, config_path, argc, argv);

   cfg = new config::ConfigFile();
   cfg->load(config_path);

   handle_args(args, *cfg);

   Simulator::setConfigFile(cfg);

   Simulator::allocate();
   Sim()->start();

   // Instrumentation
   LOG_PRINT("Start of instrumentation.");
   
   RTN_AddInstrumentFunction(routineCallback, 0);

   PIN_AddThreadStartFunction(threadStartCallback, 0);
   PIN_AddThreadFiniFunction(threadFiniCallback, 0);
   
   if (cfg->getBool("general/enable_syscall_modeling"))
   {
      PIN_AddSyscallEntryFunction(syscallEnterRunModel, 0);
      PIN_AddSyscallExitFunction(syscallExitRunModel, 0);
   }

   INS_AddInstrumentFunction(instructionCallback, 0);

   initProgressTrace();

   PIN_AddFiniFunction(ApplicationExit, 0);

   // Never returns
   LOG_PRINT("Running program...");
   PIN_StartProgram();

   return 0;
}
