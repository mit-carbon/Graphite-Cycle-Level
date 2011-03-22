#ifndef DYNAMIC_INSTRUCTION_INFO_H
#define DYNAMIC_INSTRUCTION_INFO_H

#include "instruction.h"

struct DynamicInstructionInfo
{
   enum Type
   {
      MEMORY_READ,
      MEMORY_WRITE,
      STRING,
      BRANCH,
   } type;

   union
   {
      // MEMORY
      struct
      {
         UInt64 latency;
         IntPtr addr;
      } memory_info;

      // STRING
      struct
      {
         UInt32 num_ops;
      } string_info;

      // BRANCH
      struct
      {
         bool taken;
         IntPtr target;
      } branch_info;
   };

   // ctors

   DynamicInstructionInfo()
   {
   }

   DynamicInstructionInfo(const DynamicInstructionInfo &rhs)
   {
      type = rhs.type;
      memory_info = rhs.memory_info; // "use bigger one"
   }

   static DynamicInstructionInfo createMemoryInfo(UInt64 l, IntPtr a, Operand::Direction dir)
   {
      DynamicInstructionInfo i;
      i.type = (dir == Operand::READ) ? MEMORY_READ : MEMORY_WRITE;
      i.memory_info.latency = l;
      i.memory_info.addr = a;
      return i;
   }

   static DynamicInstructionInfo createStringInfo(UInt32 count)
   {
      DynamicInstructionInfo i;
      i.type = STRING;
      i.string_info.num_ops = count;
      return i;
   }

   static DynamicInstructionInfo createBranchInfo(bool taken, IntPtr target)
   {
      DynamicInstructionInfo i;
      i.type = BRANCH;
      i.branch_info.taken = taken;
      i.branch_info.target = target;
      return i;
   }
};

#endif
