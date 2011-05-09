#pragma once

#include <vector>
using std::vector;
using std::pair;
#include "packetize.h"
#include "instruction.h"

class Core;

class PerformanceModel
{
public:
   typedef vector<pair<IntPtr,UInt32> > MemoryAccessList;

   PerformanceModel(Core* core, float frequency);
   virtual ~PerformanceModel();

   static PerformanceModel* create(Core* core);
   
   virtual bool handleInstruction(Instruction* ins, bool atomic_memory_update,
         MemoryAccessList* memory_access_list) = 0;
   virtual void handleCompletedMemoryAccess(UInt64 time, UInt32 memory_access_id) = 0;
   virtual void flushPipeline() = 0;

   virtual void outputSummary(std::ostream& os);
   
   float getFrequency() { return _frequency; }
   void updateInternalVariablesOnFrequencyChange(volatile float frequency);
   void recomputeAverageFrequency(); 
   
   UInt64 getCycleCount();
   void updateCycleCount(UInt64 cycle_count);
   void setCycleCount(UInt64 cycle_count);
   UInt64 getTime();
   void updateTime(UInt64 time);
   void setTime(UInt64 time);

   void enable() { _enabled = true; }
   void disable() { _enabled = false; }
   bool isEnabled() { return _enabled; }
   
protected:
   void frequencySummary(std::ostream &os);
   Core* getCore() { return _core; }
   
   UInt64 _cycle_count;
   
private:
   Core* _core;
   
   float _frequency;

   float _average_frequency;
   UInt64 _total_time;
   UInt64 _checkpointed_cycle_count;

   bool _enabled;
};
