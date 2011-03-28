#include "binary_semaphore.h"
#include "log.h"

#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <limits.h>

BinarySemaphore::BinarySemaphore():
   _flag(false),
   _numWaiting(0),
   _futx(0)
{}

BinarySemaphore::~BinarySemaphore()
{}

void
BinarySemaphore::wait()
{
   _lock.acquire();
   LOG_PRINT("Waiting on BinarySemaphore(%p)", this);
   while (!_flag)
   {
      _numWaiting ++;
      _futx = 0;
      LOG_PRINT("Going to Sleep on BinarySemaphore(%p)", this);

      _lock.release();

      syscall(SYS_futex, (void*) &_futx, FUTEX_WAIT, 0, NULL, NULL, 0);

      _lock.acquire();
   }

   _flag = false;
   _lock.release();
}

void
BinarySemaphore::signal()
{
   _lock.acquire();

   LOG_PRINT("Signaling on Binary Semaphore(%p)", this);
   _flag = true;
   if (_numWaiting > 0)
   {
      LOG_PRINT("Waking up a waiter on Binary Semaphore(%p)", this);
      _numWaiting --;
      _futx = 1;
      syscall(SYS_futex, (void*) &_futx, FUTEX_WAKE, 1, NULL, NULL, 0);
   }

   _lock.release();
}

void
BinarySemaphore::broadcast()
{
   _lock.acquire();

   _flag = true;
   if (_numWaiting > 0)
   {
      _futx = 1;
      syscall(SYS_futex, (void*) &_futx, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
   }

   _lock.release();
}
