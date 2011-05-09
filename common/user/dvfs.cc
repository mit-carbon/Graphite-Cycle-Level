#include "dvfs.h"
#include "simulator.h"
#include "core_manager.h"
#include "core.h"
#include "performance_model.h"
#include "routine_manager.h"

void CarbonGetCoreFrequency(float* frequency)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_GET_CORE_FREQUENCY << frequency;
   emulateRoutine(routine_info);
}

void __CarbonGetCoreFrequency(core_id_t core_id, float* frequency)
{
   Core* core = Sim()->getCoreManager()->getCoreFromID(core_id);
   // Get the core frequency
   *frequency = core->getPerformanceModel()->getFrequency();
}

void CarbonSetCoreFrequency(float* frequency)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_SET_CORE_FREQUENCY << frequency;
   emulateRoutine(routine_info);
}

void __CarbonSetCoreFrequency(core_id_t core_id, float* frequency)
{
   Core* core = Sim()->getCoreManager()->getCoreFromID(core_id);
   // 1) Core Performance Model
   // 2) Shared Memory Performance Model
   // 3) Cache Performance Model
   core->updateInternalVariablesOnFrequencyChange(*frequency);
   Config::getSingleton()->setCoreFrequency(core->getId(), *frequency);
}
