#include "perf_counter_support.h"
#include "core.h"
#include "core_manager.h"
#include "simulator.h"
#include "packet_type.h"
#include "network.h"
#include "sync_api.h"
#include "log.h"

carbon_barrier_t models_barrier;

void CarbonInitModels() 
{
   // Initialize the barrier for Carbon[Enable/Disable/Reset]Models
   CarbonBarrierInit(&models_barrier, Config::getSingleton()->getTotalCores());
}

void CarbonEnableModels()
{
   if (! Sim()->getCfg()->getBool("general/enable_models_at_startup", true))
   {
      // Acquire & Release a barrier
      CarbonBarrierWait(&models_barrier);

      if (Sim()->getCoreManager()->getCurrentCoreID() == 0)
      {
         fprintf(stderr, "[[Graphite]] --> [ Enabling Performance and Power Models ]\n");
         // Enable the models of the cores in the current process
         Simulator::enablePerformanceModels();
      }

      // Acquire & Release a barrier again
      CarbonBarrierWait(&models_barrier);
   }
}

void CarbonDisableModels()
{
   // Acquire & Release a barrier
   CarbonBarrierWait(&models_barrier);

   if (Sim()->getCoreManager()->getCurrentCoreID() == 0)
   {
      fprintf(stderr, "[[Graphite]] --> [ Disabling Performance and Power Models ]\n");
      // Disable performance models of cores in this process
      Simulator::disablePerformanceModels();
   }

   // Acquire & Release a barrier again
   CarbonBarrierWait(&models_barrier);
} 
