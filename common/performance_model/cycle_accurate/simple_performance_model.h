#pragma once

#include "cycle_accurate/performance_model.h"
#include "cycle_accurate/simple_performance_model.h"

namespace CycleAccurate
{

class SimplePerformanceModel : public PerformanceModel
{
public:
   SimplePerformanceModel(Core* core, float frequency);
   ~SimplePerformanceModel();

   void queueInstruction(Instruction* i, bool atomic_memory_update, MemoryAccessList* memory_access_list);
   void processDynamicInstructionInfo(DynamicInstructionInfo& info);
   void processNextInstruction();

private:
   class InstructionStatus
   {
   public:
      InstructionStatus(Instruction* instruction, bool atomic_memory_update, MemoryAccessList* memory_access_list);
      ~InstructionStatus();

      Instruction* _instruction; // Created at instrumentation time (SHOULD NOT BE DELETED !!)
      bool _atomic_memory_update;
      MemoryAccessList* _memory_access_list; // Created at analysis time
      UInt64 _time;
      UInt32 _curr_memory_operand_num;
      UInt32 _total_read_memory_operands;
      UInt32 _total_write_memory_operands;
      UInt32 _total_memory_operands;
   };

   InstructionStatus* _curr_instruction_status;
   std::queue<InstructionStatus*> _instruction_status_queue;
   Lock _instruction_status_queue_lock;
   bool _waiting;

   bool getNextInstruction();
   void processInstruction();
   void issueNextMemoryRequest();
   void completeInstruction();
};

}
