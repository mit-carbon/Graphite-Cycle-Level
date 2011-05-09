#include "simulator.h"
#include "core.h"
#include "performance_model.h"
#include "simple_performance_model.h"
#include "clock_converter.h"
#include "config.h"

PerformanceModel*
PerformanceModel::create(Core* core)
{
   volatile float frequency = Config::getSingleton()->getCoreFrequency(core->getId());
   string core_model = Config::getSingleton()->getCoreType(core->getId());

   if (core_model == "simple")
      return new SimplePerformanceModel(core, frequency);
   else
      LOG_PRINT_ERROR("Invalid perf model type: %s", core_model.c_str());
   return (PerformanceModel*) NULL;
}

// Public Interface
PerformanceModel::PerformanceModel(Core *core, float frequency)
   : _cycle_count(0)
   , _core(core)
   , _frequency(frequency)
   , _average_frequency(0.0)
   , _total_time(0)
   , _checkpointed_cycle_count(0)
   , _enabled(false)
{
   // Initialize Performance Counters
   _total_instructions_executed = 0;
   _total_instructions_issued = 0;

   _max_outstanding_instructions = (UInt64) Sim()->getCfg()->getInt("general/max_outstanding_instructions", 1);
}

PerformanceModel::~PerformanceModel()
{}

void
PerformanceModel::outputSummary(ostream& os)
{
   // Frequency Summary
   frequencySummary(os);
}

void
PerformanceModel::frequencySummary(ostream& os)
{
   os << "    Total Instructions: " << _total_instructions_executed << endl;
   os << "    Completion Time: " << (UInt64) (((float) _cycle_count) / _frequency) << endl;
   os << "    Average Frequency: " << _average_frequency << endl;
}

// This function is called:
//  * Whenever frequency is changed
void
PerformanceModel::updateInternalVariablesOnFrequencyChange(float frequency)
{
   recomputeAverageFrequency();
   
   float old_frequency = _frequency;
   float new_frequency = frequency;
   
   _checkpointed_cycle_count = (UInt64) (((double) _cycle_count / old_frequency) * new_frequency);
   _cycle_count = _checkpointed_cycle_count;
   _frequency = new_frequency;
}

// This function is called:
//  * On thread exit
//  * Whenever frequency is changed
void
PerformanceModel::recomputeAverageFrequency()
{
   double cycles_elapsed = (double) (_cycle_count - _checkpointed_cycle_count);
   double total_cycles_executed = (_average_frequency * _total_time) + cycles_elapsed;
   double total_time_taken = _total_time + (cycles_elapsed / _frequency);

   _average_frequency = total_cycles_executed / total_time_taken;
   _total_time = (UInt64) total_time_taken;
}

UInt64
PerformanceModel::getCycleCount()
{
   return _cycle_count;
}

// This function is called:
//  * On message recv
//  * Indirectly, on thread sync
void
PerformanceModel::updateCycleCount(UInt64 cycle_count)
{
   assert(cycle_count >= _cycle_count);
   _cycle_count = cycle_count;
}

// This function is called:
//  * On thread start
void
PerformanceModel::setCycleCount(UInt64 cycle_count)
{
   _checkpointed_cycle_count = cycle_count;
   _cycle_count = cycle_count;
}

UInt64
PerformanceModel::getTime()
{
   return convertCycleCount(getCycleCount(), _frequency, 1.0);
}

// This function is called:
//  * On synchronization
void
PerformanceModel::updateTime(UInt64 time)
{
   updateCycleCount(convertCycleCount(time, 1.0, _frequency));
}

void
PerformanceModel::setTime(UInt64 time)
{
   setCycleCount(convertCycleCount(time, 1.0, _frequency));
}
