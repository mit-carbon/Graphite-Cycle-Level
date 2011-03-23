#include "binary_semaphore.h"

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
   while (!_flag)
   {
      _numWaiting ++;
      _futx = 0;

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

   _flag = true;
   if (_numWaiting > 0)
   {
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
