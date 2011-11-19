#include <sys/syscall.h>
#include "syscall_manager.h"
#include "core.h"
#include "config.h"
#include "simulator.h"
#include "thread_manager.h"
#include "network.h"
#include "log.h"

SyscallManager::SyscallManager()
{}

SyscallManager::~SyscallManager()
{}


void
SyscallManager::handleSyscall(UInt64 time, core_id_t core_id, IntPtr syscall_number, syscall_args_t args)
{
   // Acquire Lock
   ScopedLock sl(_lock);

   LOG_PRINT("Syscall: %i from core(%i), Time(%llu)", (int) syscall_number, core_id, time);

   switch (syscall_number)
   {
   case SYS_futex:
      handleFutexCall(time, core_id, args);
      break;

   default:
      LOG_PRINT_ERROR("Unhandled syscall number(%i) from %i", (int) syscall_number, core_id);
      break;
   }

   LOG_PRINT("Finished syscall: %i", syscall_number);
}

void
SyscallManager::handleFutexCall(UInt64 curr_time, core_id_t core_id, syscall_args_t args)
{
   int* addr1 = (int*) args.arg0;
   int op = (int) args.arg1;
   int val1 = (int) args.arg2;
   void* timeout = (void*) args.arg3;
   int* addr2 = (int*) args.arg4;
   int val3 = (int) args.arg5;

   LOG_PRINT("Futex syscall: addr1(%#x), op(%u), val1(%i), addr2(%#x), val3(%i)", addr1, op, val1, addr2, val3);

   // Right now, we handle only a subset of the functionality

   LOG_ASSERT_ERROR((op == FUTEX_WAIT)          || (op == (FUTEX_WAIT | FUTEX_PRIVATE_FLAG))       ||
                    (op == FUTEX_WAKE)          || (op == (FUTEX_WAKE | FUTEX_PRIVATE_FLAG))       ||
                    (op == FUTEX_WAKE_OP)       || (op == (FUTEX_WAKE_OP | FUTEX_PRIVATE_FLAG))    ||
                    (op == FUTEX_CMP_REQUEUE)   || (op == (FUTEX_CMP_REQUEUE | FUTEX_PRIVATE_FLAG)),
                    "op = %#x", op);

   if ((op == FUTEX_WAIT) || (op == (FUTEX_WAIT | FUTEX_PRIVATE_FLAG)))
   {
      LOG_ASSERT_ERROR(!timeout, "timeout not-NULL(%p)", timeout);
      futexWait(core_id, addr1, val1, curr_time); 
   }
   else if ((op == FUTEX_WAKE) || (op == (FUTEX_WAKE | FUTEX_PRIVATE_FLAG)))
   {
      futexWake(core_id, addr1, val1, curr_time);
   }
   else if ((op == FUTEX_WAKE_OP) || (op == (FUTEX_WAKE_OP | FUTEX_PRIVATE_FLAG)))
   {
      int val2 = (long int) timeout;
      futexWakeOp(core_id, addr1, val1, val2, addr2, val3, curr_time);
   }
   else if ((op == FUTEX_CMP_REQUEUE) || (op == (FUTEX_CMP_REQUEUE | FUTEX_PRIVATE_FLAG)))
   {
      int val2 = (long int) timeout;
      futexCmpRequeue(core_id, addr1, val1, val2, addr2, val3, curr_time);
   }
}

// Futex related functions
void
SyscallManager::futexWait(core_id_t core_id, int *addr, int val, UInt64 curr_time)
{
   LOG_PRINT("Futex Wait");

   SimFutex *sim_futex = &_futexes[(IntPtr) addr];
 
   int curr_val = *addr;
   if (val != curr_val)
   {
      Sim()->getThreadInterface(core_id)->sendSimReply(curr_time, EWOULDBLOCK);
   }
   else
   {
      sim_futex->enqueueWaiter(core_id);
   }
}

void
SyscallManager::futexWake(core_id_t core_id, int *addr, int val, UInt64 curr_time)
{
   LOG_PRINT("Futex Wake");

   int num_procs_woken_up = __futexWake(addr, val, curr_time);
   Sim()->getThreadInterface(core_id)->sendSimReply(curr_time, num_procs_woken_up);
}

void
SyscallManager::futexWakeOp(core_id_t core_id, int *addr1, int val1, int val2, int* addr2, int val3, UInt64 curr_time)
{
   int OP = (val3 >> 28) & 0xf;
   int CMP = (val3 >> 24) & 0xf;
   int OPARG = (val3 >> 12) & 0xfff;
   int CMPARG = (val3) & 0xfff;

   int num_procs_woken_up = 0;

   // Get the old value of addr2
   int oldval = *addr2;
   
   int newval = 0;
   switch (OP)
   {
   case FUTEX_OP_SET:
      newval = OPARG;
      break;

   case FUTEX_OP_ADD:
      newval = oldval + OPARG;
      break;

   case FUTEX_OP_OR:
      newval = oldval | OPARG;
      break;

   case FUTEX_OP_ANDN:
      newval = oldval & (~OPARG);
      break;

   case FUTEX_OP_XOR:
      newval = oldval ^ OPARG;
      break;

   default:
      LOG_PRINT_ERROR("Futex syscall: FUTEX_WAKE_OP: Unhandled OP(%i)", OP);
      break;
   }
   
   // Write the newval into addr2
   *addr2 = newval;

   // Wake upto val1 threads waiting on the first futex
   num_procs_woken_up += __futexWake(addr1, val1, curr_time);

   bool condition = false;
   switch (CMP)
   {
   case FUTEX_OP_CMP_EQ:
      condition = (oldval == CMPARG);
      break;

   case FUTEX_OP_CMP_NE:
      condition = (oldval != CMPARG);
      break;

   case FUTEX_OP_CMP_LT:
      condition = (oldval < CMPARG);
      break;

   case FUTEX_OP_CMP_LE:
      condition = (oldval <= CMPARG);
      break;

   case FUTEX_OP_CMP_GT:
      condition = (oldval > CMPARG);
      break;

   case FUTEX_OP_CMP_GE:
      condition = (oldval >= CMPARG);
      break;

   default:
      LOG_PRINT_ERROR("Futex syscall: FUTEX_WAKE_OP: Unhandled CMP(%i)", CMP);
      break;
   }
   
   // Wake upto val2 threads waiting on the second futex if the condition is true
   if (condition)
      num_procs_woken_up += __futexWake(addr2, val2, curr_time);

   // Send reply to the requester
   Sim()->getThreadInterface(core_id)->sendSimReply(curr_time, num_procs_woken_up);
}

void
SyscallManager::futexCmpRequeue(core_id_t core_id, int *addr1, int val1, int val2,
                                int *addr2, int val3, UInt64 curr_time)
{
   LOG_PRINT("Futex CMP_REQUEUE");

   int curr_val = *addr1;
   if (val3 != curr_val)
   {
      Sim()->getThreadInterface(core_id)->sendSimReply(curr_time, EWOULDBLOCK);
   }
   else
   {
      SimFutex *sim_futex = &_futexes[(IntPtr) addr1];
      int num_procs_woken_up_or_requeued = 0;

      num_procs_woken_up_or_requeued += __futexWake(addr1, val1, curr_time);

      SimFutex *requeue_futex = &_futexes[(IntPtr) addr2];

      for (int i = 0; i < val2; i++)
      {
         core_id_t waiter = sim_futex->dequeueWaiter();
         if (waiter == INVALID_CORE_ID)
            break;

         num_procs_woken_up_or_requeued ++;

         requeue_futex->enqueueWaiter(waiter);
      }

      Sim()->getThreadInterface(core_id)->sendSimReply(curr_time, num_procs_woken_up_or_requeued);
   }
}

int
SyscallManager::__futexWake(int* addr, int val, UInt64 curr_time)
{
   SimFutex *sim_futex = &_futexes[(IntPtr) addr];

   int num_procs_woken_up = 0;
   for (int i = 0; i < val; i++)
   {
      core_id_t waiter = sim_futex->dequeueWaiter();
      if (waiter == INVALID_CORE_ID)
         break;

      num_procs_woken_up ++;

      Sim()->getThreadInterface(waiter)->sendSimReply(curr_time, 0);
   }

   return num_procs_woken_up;
}


// SimFutex
SimFutex::SimFutex()
{}

SimFutex::~SimFutex()
{
   assert(_waiting.empty());
}

void
SimFutex::enqueueWaiter(core_id_t core_id)
{
   _waiting.push(core_id);
}

core_id_t
SimFutex::dequeueWaiter()
{
   if (_waiting.empty())
   {
      return INVALID_CORE_ID;
   }
   else
   {
      core_id_t core_id = _waiting.front();
      _waiting.pop();
      return core_id;
   }
}
