#include "cycle_accurate/performance_model.h"
#include "cycle_accurate/simple_performance_model.h"
#include "simulator.h"
#include "core_manager.h"
#include "config.h"
#include "core.h"
#include "fxsupport.h"
#include "utils.h"

namespace CycleAccurate
{

::PerformanceModel*
PerformanceModel::create(Core* core)
{
   volatile float frequency = Config::getSingleton()->getCoreFrequency(core->getId());
   string core_model = Config::getSingleton()->getCoreType(core->getId());

   if ((core_model == "simple") || (core_model == "magic"))
      return new SimplePerformanceModel(core, frequency);
   else
      LOG_PRINT_ERROR("Invalid perf model type: %s", core_model.c_str());
   return (PerformanceModel*) NULL;
}

// Public Interface
PerformanceModel::PerformanceModel(Core *core, float frequency)
   : ::PerformanceModel(core, frequency)
{}

PerformanceModel::~PerformanceModel()
{}

}
