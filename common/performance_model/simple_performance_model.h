#pragma once

#include "performance_model.h"
#include "instruction.h"

class SimplePerformanceModel : public PerformanceModel
{
public:
   SimplePerformanceModel(Core* core, float frequency);
   ~SimplePerformanceModel();

   bool handleInstruction(Instruction* ins,
                          bool atomic_memory_update,
                          MemoryAccessList* memory_access_list);
   void handleCompletedMemoryAccess(UInt64 time, UInt32 memory_access_id);
   void flushPipeline() {}

   void outputSummary(std::ostream& os);

private:
   class InstructionStatus
   {
   public:
      InstructionStatus();
      ~InstructionStatus();
      
      void update(UInt64 time, Instruction* instruction, bool atomic_memory_update, MemoryAccessList* memory_access_list);

      UInt64 _cycle_count;
      Instruction* _instruction; // Created at instrumentation time (SHOULD NOT BE DELETED !!)
      bool _atomic_memory_update;
      MemoryAccessList* _memory_access_list; // Created at analysis time
      UInt32 _curr_memory_operand_num;
      UInt32 _total_read_memory_operands;
      UInt32 _total_write_memory_operands;
      UInt32 _total_memory_operands;
   };

   InstructionStatus _curr_instruction_status;
   UInt32 _last_memory_access_id;
   static const UInt32 SCRATCHPAD_SIZE = 1024;
   Byte _data_buffer[SCRATCHPAD_SIZE]; // Only 1 outstanding memory request allowed in the simple core model
   Byte* _large_data_buffer;

   bool issueNextMemoryRequest();
   void completeInstruction();
};
