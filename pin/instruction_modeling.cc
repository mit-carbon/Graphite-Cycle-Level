#include "instruction_modeling.h"
#include "simulator.h"
#include "core_manager.h"
#include "core.h"
#include "performance_model.h"
#include "opcodes.h"

void handleInstruction(Instruction *instruction, bool atomic_memory_update, UInt32 num_memory_args, ...)
{
   LOG_PRINT("handleInstruction(%p,%s,%u)", instruction, atomic_memory_update ? "TRUE" : "FALSE", num_memory_args);

   Core* core = Sim()->getCoreManager()->getCurrentCore();
   assert(core);

   if (core->getPerformanceModel()->isEnabled())
   {
      UnstructuredBuffer* instruction_info = new UnstructuredBuffer();
      
      (*instruction_info) << instruction;
      (*instruction_info) << atomic_memory_update;

      va_list memory_args;
      va_start(memory_args, num_memory_args);
      PerformanceModel::MemoryAccessList* memory_access_list = new PerformanceModel::MemoryAccessList();
      for (UInt32 i = 0; i < num_memory_args; i++)
      {
         IntPtr address = va_arg(memory_args, IntPtr);
         UInt32 size = va_arg(memory_args, UInt32);
        
         memory_access_list->push_back(make_pair(address,size));
         LOG_PRINT("Address(0x%llx), Size(%u)", address, size);
      }
      va_end(memory_args);

      (*instruction_info) << memory_access_list;

      // Send request to sim thread
      AppRequest app_request(AppRequest::HANDLE_INSTRUCTION, instruction_info);
      Sim()->getThreadInterface(core->getId())->sendAppRequest(app_request);
      
      // Increment the number of issued instructions
      core->getPerformanceModel()->incrTotalInstructionsIssued();

      UInt64 total_instructions_issued = core->getPerformanceModel()->getTotalInstructionsIssued();
      if ( (total_instructions_issued % core->getPerformanceModel()->getMaxOutstandingInstructions()) == 0 )
      {
         // Receive Reply after every 'n' instructions are executed
         SimReply sim_reply = Sim()->getThreadInterface(core->getId())->recvSimReply();
         LOG_ASSERT_ERROR(sim_reply == core->getPerformanceModel()->getMaxOutstandingInstructions(),
               "Sim Reply(%llu), Max Outstanding Instructions(%llu)",
               sim_reply, core->getPerformanceModel()->getMaxOutstandingInstructions());
      }
   }
}

void fillOperandListMemOps(OperandList* list, INS ins)
{
   // NOTE: This code is written to reflect rewriteStackOp and
   // rewriteMemOp etc from redirect_memory.cc and it MUST BE
   // MAINTAINED to reflect that code.

   // mem ops
   if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins))
   {
      if (INS_IsMemoryRead(ins))
         list->push_back(Operand(Operand::MEMORY, 0, Operand::READ));

      if (INS_HasMemoryRead2(ins))
         list->push_back(Operand(Operand::MEMORY, 0, Operand::READ));

      if (INS_IsMemoryWrite(ins))
         list->push_back(Operand(Operand::MEMORY, 0, Operand::WRITE));
   }
}

UInt32 fillMemInfo(IARGLIST& iarg_memory_info, INS ins)
{
   UInt32 num_memory_args = 0;
   if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins))
   {
      if (INS_IsMemoryRead(ins))
      {
         num_memory_args ++;
         IARGLIST_AddArguments(iarg_memory_info, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
      }
      if (INS_HasMemoryRead2(ins))
      {
         num_memory_args ++;
         IARGLIST_AddArguments(iarg_memory_info, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_END);
      }
      if (INS_IsMemoryWrite(ins))
      {
         num_memory_args ++;
         IARGLIST_AddArguments(iarg_memory_info, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
      }
   }
   return num_memory_args;
}

void fillOperandList(OperandList* list, INS ins)
{
   // memory
   fillOperandListMemOps(list, ins);

   // for handling register operands
   unsigned int max_read_regs = INS_MaxNumRRegs(ins);
   unsigned int max_write_regs = INS_MaxNumRRegs(ins);

   for (unsigned int i = 0; i < max_read_regs; i++)
   {
      if (REG_valid(INS_RegR(ins, i)))
         list->push_back(Operand(Operand::REG, INS_RegR(ins, i), Operand::READ));
   }

   for (unsigned int i = 0; i < max_write_regs; i++)
   {
      if (REG_valid(INS_RegW(ins, i)))
         list->push_back(Operand(Operand::REG, INS_RegW(ins, i), Operand::WRITE));
   }

   // immediate
   for (unsigned int i = 0; i < INS_OperandCount(ins); i++)
   {
      if (INS_OperandIsImmediate(ins, i))
      {
         list->push_back(Operand(Operand::IMMEDIATE, INS_OperandImmediate(ins, i), Operand::READ));
      }
   }
}

void addInstructionModeling(INS ins)
{
   OperandList list;
   fillOperandList(&list, ins);

   Instruction* instruction;

   // Now handle instructions which have a static cost
   switch(INS_Opcode(ins))
   {
   case OPCODE_DIV:
      instruction = new ArithInstruction(INST_DIV, list);
      break;
   case OPCODE_MUL:
      instruction = new ArithInstruction(INST_MUL, list);
      break;
   case OPCODE_FDIV:
      instruction = new ArithInstruction(INST_FDIV, list);
      break;
   case OPCODE_FMUL:
      instruction = new ArithInstruction(INST_FMUL, list);
      break;
   default:
      instruction = new GenericInstruction(list);
      break;
   }

   // Set Instruction Address
   instruction->setAddress(INS_Address(ins));

   // Add Memory Read/Write Addresses/Size
   IARGLIST iarg_memory_info = IARGLIST_Alloc();
   UInt32 num_memory_args = fillMemInfo(iarg_memory_info, ins);
   
   INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(handleInstruction),
         IARG_PTR, instruction,
         IARG_BOOL, INS_IsAtomicUpdate(ins),
         IARG_UINT32, num_memory_args,
         IARG_IARGLIST, iarg_memory_info,
         IARG_END);

   IARGLIST_Free(iarg_memory_info);
}
