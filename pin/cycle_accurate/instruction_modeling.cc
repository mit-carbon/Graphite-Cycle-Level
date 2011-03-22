#include "cycle_accurate/instruction_modeling.h"

#include "simulator.h"
#include "cycle_accurate/performance_model.h"
#include "opcodes.h"
#include "core_manager.h"
#include "core.h"

namespace CycleAccurate
{

void handleInstruction(Instruction *instruction, bool is_atomic_update, UInt32 num_memory_args, ...)
{
   PerformanceModel* prfmdl =
      (PerformanceModel*) Sim()->getCoreManager()->getCurrentCore()->getPerformanceModel();

   va_list memory_args;
   va_start(memory_args, num_memory_args);
   PerformanceModel::MemoryAccessList* memory_access_list = new PerformanceModel::MemoryAccessList();
   for (UInt32 i = 0; i < num_memory_args; i++)
   {
      IntPtr address = va_arg(memory_args, IntPtr);
      UInt32 size = va_arg(memory_args, UInt32);
      memory_access_list->push_back(make_pair<IntPtr,UInt32>(address, size));
   }
   va_end(memory_args);

   prfmdl->queueInstruction(instruction, is_atomic_update, memory_access_list);
}

void handleBranch(bool taken, IntPtr target)
{
   assert(Sim() && Sim()->getCoreManager() && Sim()->getCoreManager()->getCurrentCore());
   PerformanceModel* prfmdl =
      (PerformanceModel*) Sim()->getCoreManager()->getCurrentCore()->getPerformanceModel();

   // FIXME: Correct this
   DynamicInstructionInfo info = DynamicInstructionInfo::createBranchInfo(taken, target);
   prfmdl->pushDynamicInstructionInfo(info);
}

void fillOperandListMemOps(OperandList *list, INS ins)
{
   // NOTE: This code is written to reflect rewriteStackOp and
   // rewriteMemOp etc from redirect_memory.cc and it MUST BE
   // MAINTAINED to reflect that code.

   // mem ops
   if (INS_IsMemoryRead (ins) || INS_IsMemoryWrite (ins))
   {
      if (INS_IsMemoryRead (ins))
         list->push_back(Operand(Operand::MEMORY, 0, Operand::READ));

      if (INS_HasMemoryRead2 (ins))
         list->push_back(Operand(Operand::MEMORY, 0, Operand::READ));

      if (INS_IsMemoryWrite (ins))
         list->push_back(Operand(Operand::MEMORY, 0, Operand::WRITE));
   }
}

void fillMemInfo(IARGLIST& iarg_memory_info, INS ins)
{
   if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins))
   {
      if (INS_IsMemoryRead(ins))
         IARGLIST_AddArguments(iarg_memory_info, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
      if (INS_HasMemoryRead2(ins))
         IARGLIST_AddArguments(iarg_memory_info, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_END);
      if (INS_IsMemoryWrite(ins))
         IARGLIST_AddArguments(iarg_memory_info, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
   }
}

void fillOperandList(OperandList *list, INS ins)
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

   // branches
   if (INS_IsBranch(ins) && INS_HasFallThrough(ins))
   {
      instruction = new BranchInstruction(list);

      INS_InsertCall(
         ins, IPOINT_TAKEN_BRANCH, (AFUNPTR) handleBranch,
         IARG_BOOL, TRUE,
         IARG_BRANCH_TARGET_ADDR,
         IARG_END);

      INS_InsertCall(
         ins, IPOINT_AFTER, (AFUNPTR) handleBranch,
         IARG_BOOL, FALSE,
         IARG_BRANCH_TARGET_ADDR,
         IARG_END);
   }

   // Now handle instructions which have a static cost
   else
   {
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
   }

   // Set Instruction Address
   instruction->setAddress(INS_Address(ins));

   // Add Memory Read/Write Addresses/Size
   IARGLIST iarg_memory_info = IARGLIST_Alloc();
   fillMemInfo(iarg_memory_info, ins);
   
   INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(handleInstruction),
         IARG_PTR, instruction,
         IARG_IARGLIST, iarg_memory_info,
         IARG_END);

   IARGLIST_Free(iarg_memory_info);
}

}
