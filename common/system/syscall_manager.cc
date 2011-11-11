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

   LOG_PRINT("Syscall: %d from core(%i), Time(%llu)", syscall_number, core_id, time);

   switch (syscall_number)
   {
   case SYS_futex:
      handleFutexCall(time, core_id, args);
      break;

   default:
      LOG_PRINT_ERROR("Unhandled syscall number: %i from %i", (int) syscall_number, core_id);
      break;
   }

   LOG_PRINT("Finished syscall: %d", syscall_number);
}

void
SyscallManager::handleFutexCall(UInt64 curr_time, core_id_t core_id, syscall_args_t args)
{
   int *uaddr = (int*) args.arg0;
   int op = (int) args.arg1;
   int val = (int) args.arg2;
   const struct timespec *timeout = (const struct timespec*) args.arg3;
   int *uaddr2 = (int*) args.arg4;
   int val3 = (int) args.arg5;

   LOG_PRINT("Futex syscall: uaddr(0x%llx), op(%u), val(%u)", uaddr, op, val);

   // Right now, we handle only a subset of the functionality

#ifdef KERNEL_LENNY
   LOG_ASSERT_ERROR((op == FUTEX_WAIT) || (op == (FUTEX_WAIT | FUTEX_PRIVATE_FLAG)) || \
                    (op == FUTEX_WAKE) || (op == (FUTEX_WAKE | FUTEX_PRIVATE_FLAG)) || \
                    (op == FUTEX_CMP_REQUEUE) || (op == (FUTEX_CMP_REQUEUE | FUTEX_PRIVATE_FLAG)),
                    "op = 0x%x", op);
   if ((op == FUTEX_WAIT) || (op == (FUTEX_WAIT | FUTEX_PRIVATE_FLAG)))
   {
      LOG_ASSERT_ERROR(!timeout, "timeout(%p)", timeout);
   }
#endif

#ifdef KERNEL_ETCH
   LOG_ASSERT_ERROR(((op == FUTEX_WAIT) || (op == FUTEX_WAKE)), "op = %u", op);
   if (op == FUTEX_WAIT)
   {
      LOG_ASSERT_ERROR(!timeout, "timeout(%p)", timeout);
   }
#endif

   // Get the actual value from application memory
   int act_val = *uaddr;

#ifdef KERNEL_LENNY
   if ((op == FUTEX_WAIT) || (op == (FUTEX_WAIT | FUTEX_PRIVATE_FLAG)))
   {
      futexWait(core_id, uaddr, val, act_val, curr_time); 
   }
   else if ((op == FUTEX_WAKE) || (op == (FUTEX_WAKE | FUTEX_PRIVATE_FLAG)))
   {
      futexWake(core_id, uaddr, val, curr_time);
   }
   else if ((op == FUTEX_CMP_REQUEUE) || (op == (FUTEX_CMP_REQUEUE | FUTEX_PRIVATE_FLAG)))
   {
      futexCmpRequeue(core_id, uaddr, val, uaddr2, val3, act_val, curr_time);
   }
#endif
   
#ifdef KERNEL_ETCH
   if (op == FUTEX_WAIT)
   {
      futexWait(core_id, uaddr, val, act_val, curr_time); 
   }
   else if (op == FUTEX_WAKE)
   {
      futexWake(core_id, uaddr, val, curr_time);
   }
#endif

}

// Futex related functions
void
SyscallManager::futexWait(core_id_t core_id, int *uaddr, int val, int act_val, UInt64 curr_time)
{
   LOG_PRINT("Futex Wait");
   SimFutex *sim_futex = &_futexes[(IntPtr) uaddr];
  
   if (val != act_val)
   {
      Sim()->getThreadInterface(core_id)->sendSimReply(curr_time, (IntPtr) EWOULDBLOCK);
   }
   else
   {
      sim_futex->enqueueWaiter(core_id);
   }
}

void
SyscallManager::futexWake(core_id_t core_id, int *uaddr, int val, UInt64 curr_time)
{
   LOG_PRINT("Futex Wake");
   SimFutex *sim_futex = &_futexes[(IntPtr) uaddr];
   int num_procs_woken_up = 0;

   for (int i = 0; i < val; i++)
   {
      core_id_t waiter = sim_futex->dequeueWaiter();
      if (waiter == INVALID_CORE_ID)
         break;

      num_procs_woken_up ++;

      Sim()->getThreadInterface(waiter)->sendSimReply(curr_time, 0);
   }

   Sim()->getThreadInterface(core_id)->sendSimReply(curr_time, num_procs_woken_up);

}

void
SyscallManager::futexCmpRequeue(core_id_t core_id, int *uaddr, int val,
                                int *uaddr2, int val3, int act_val, UInt64 curr_time)
{
   LOG_PRINT("Futex CMP_REQUEUE");
   SimFutex *sim_futex = &_futexes[(IntPtr) uaddr];
   int num_procs_woken_up = 0;

   if (val3 != act_val)
   {
      Sim()->getThreadInterface(core_id)->sendSimReply(curr_time, (IntPtr) EAGAIN);
   }
   else
   {
      for (int i = 0; i < val; i++)
      {
         core_id_t waiter = sim_futex->dequeueWaiter();
         if (waiter == INVALID_CORE_ID)
            break;

         num_procs_woken_up ++;

         Sim()->getThreadInterface(waiter)->sendSimReply(curr_time, (IntPtr) 0);
      }

      SimFutex *requeue_futex = &_futexes[(IntPtr) uaddr2];

      while (true)
      {
         // dequeueWaiter changes the thread state to
         // RUNNING, which is changed back to STALLED 
         // by enqueueWaiter. Since only the MCP uses this state
         // this should be okay. 
         core_id_t waiter = sim_futex->dequeueWaiter();
         if (waiter == INVALID_CORE_ID)
            break;

         requeue_futex->enqueueWaiter(waiter);
      }

      Sim()->getThreadInterface(core_id)->sendSimReply(curr_time, num_procs_woken_up);
   }
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
