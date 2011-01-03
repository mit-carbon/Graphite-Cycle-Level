#pragma once

#include "lock.h"

class BinarySemaphore
{
   private:
      bool _flag;
      int _numWaiting;
      int _futx;
      Lock _lock;

   public:
      BinarySemaphore();
      ~BinarySemaphore();

      void wait();
      void signal();
      void broadcast();
};
