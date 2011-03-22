#pragma once

#include <vector>
#include "../performance_model.h"

namespace CycleAccurate
{

class PerformanceModel : public ::PerformanceModel
{
public:
   // Memory Access List of an Instruction
   typedef std::vector<std::pair<IntPtr,UInt32> > MemoryAccessList;

   PerformanceModel(Core* core, float frequency);
   virtual ~PerformanceModel();

   static ::PerformanceModel* create(Core* core);
   
   virtual void queueInstruction(Instruction* i, bool is_atomic_update, MemoryAccessList* memory_access_list) = 0;
   // FIXME: Determine if we need 'time' as the 1st argument
   virtual void processDynamicInstructionInfo(DynamicInstructionInfo& info) = 0;
   virtual void processNextInstruction() = 0;

   void outputSummary(std::ostream& os) {}

private:
   void handleInstruction(Instruction* instruction) {}
};

}
