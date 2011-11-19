#pragma once

#include <map>
#include <queue>

// For futexes
#include <linux/futex.h>
#include <sys/time.h>
#include <errno.h>
#include "packetize.h"
#include "fixed_types.h"
#include "network.h"
#include "lock.h"

// Special Class to Handle Futexes
class SimFutex
{
public:
   SimFutex();
   ~SimFutex();
   void enqueueWaiter(core_id_t core_id);
   core_id_t dequeueWaiter();

private:
   typedef std::queue<core_id_t> ThreadQueue;
   ThreadQueue _waiting;
};

class SyscallManager
{
public:
   struct syscall_args_t
   {
       IntPtr arg0;
       IntPtr arg1;
       IntPtr arg2;
       IntPtr arg3;
       IntPtr arg4;
       IntPtr arg5;
   };

   SyscallManager();
   ~SyscallManager();

   void handleSyscall(UInt64 time, core_id_t core_id, IntPtr syscall_number, syscall_args_t args);

private:
   void handleFutexCall(UInt64 curr_time, core_id_t core_id, syscall_args_t args);

   Lock _lock;

   // Handling Futexes 
   void futexWait(core_id_t core_id, int *addr, int val, UInt64 curr_time);
   void futexWake(core_id_t core_id, int *addr, int val, UInt64 curr_time);
   void futexWakeOp(core_id_t core_id, int *addr1, int val1, int val2, int* addr2, int val3, UInt64 curr_time);
   void futexCmpRequeue(core_id_t core_id, int *addr1, int val1, int val2, int *addr2, int val3, UInt64 curr_time);
   int __futexWake(int* addr, int val, UInt64 curr_time);

   // Handling Futexes
   typedef std::map<IntPtr, SimFutex> FutexMap;
   FutexMap _futexes;
};
