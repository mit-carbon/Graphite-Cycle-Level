#include "lite/memory_modeling.h"
#include "simulator.h"
#include "core_manager.h"
#include "core.h"

namespace Lite
{

void addMemoryModeling(INS ins)
{
   if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins))
   {
      if (INS_IsMemoryRead(ins))
      {
         INS_InsertCall(ins, IPOINT_BEFORE,
               AFUNPTR(handleMemoryRead),
               IARG_BOOL, INS_IsAtomicUpdate(ins),
               IARG_MEMORYREAD_EA,
               IARG_MEMORYREAD_SIZE,
               IARG_END);
      }
      if (INS_HasMemoryRead2(ins))
      {
         LOG_ASSERT_ERROR(!INS_IsAtomicUpdate(ins), "Atomic Instruction has 2 read operands");

         INS_InsertCall(ins, IPOINT_BEFORE,
               AFUNPTR(handleMemoryRead),
               IARG_BOOL, false,
               IARG_MEMORYREAD2_EA,
               IARG_MEMORYREAD_SIZE,
               IARG_END);
      }
      if (INS_IsMemoryWrite(ins))
      {
         IPOINT ipoint = INS_HasFallThrough(ins) ? IPOINT_AFTER : IPOINT_TAKEN_BRANCH;
         INS_InsertCall(ins, ipoint,
               AFUNPTR(handleMemoryWrite),
               IARG_BOOL, INS_IsAtomicUpdate(ins),
               IARG_MEMORYWRITE_EA,
               IARG_MEMORYWRITE_SIZE,
               IARG_END);
      }
   }
}

void handleMemoryRead(bool is_atomic_update, IntPtr read_address, UInt32 read_data_size)
{
   // FIXME: May want to do something intelligent here. Dont know how time consuming this operation is!
   Byte read_data_buf[read_data_size];

   Core* core = Sim()->getCoreManager()->getCurrentCore();
   core->initiateMemoryAccess(0 /* time */,
         MemComponent::L1_DCACHE,
         (is_atomic_update) ? Core::LOCK : Core::NONE,
         (is_atomic_update) ? Core::READ_EX : Core::READ,
         read_address,
         read_data_buf,
         read_data_size,
         true);
}

void handleMemoryWrite(bool is_atomic_update, IntPtr write_address, UInt32 write_data_size)
{
   Core* core = Sim()->getCoreManager()->getCurrentCore();
   core->initiateMemoryAccess(0 /* time */,
         MemComponent::L1_DCACHE,
         (is_atomic_update) ? Core::UNLOCK : Core::NONE,
         Core::WRITE,
         write_address,
         (Byte*) write_address,
         write_data_size,
         true);
}

}
