#pragma once

#include <queue>
#include <vector>
#include <limits.h>
#include <string.h>

#include "lock.h"
#include "sync_api.h"
#include "stable_iterator.h"

class SimMutex
{
public:
   static const core_id_t NO_OWNER = UINT_MAX;

   SimMutex();
   ~SimMutex();

   // returns true if this thread now owns the lock
   bool lock(core_id_t core_id);

   // returns the next owner of the lock so that it can be signaled by
   // the server
   core_id_t unlock(core_id_t core_id);

private:
   typedef std::queue<core_id_t> ThreadQueue;

   ThreadQueue _waiting;
   core_id_t _owner;
};

class SimCond
{
public:
   typedef std::vector<core_id_t> WakeupList;

   SimCond();
   ~SimCond();

   // returns the thread that gets woken up when the mux is unlocked
   core_id_t wait(UInt64 time, core_id_t core_id, StableIterator<SimMutex> & it);
   core_id_t signal(UInt64 time, core_id_t core_id);
   void broadcast(UInt64 time, core_id_t core_id, WakeupList &woken);

private:
   class CondWaiter
   {
      public:
         CondWaiter(core_id_t core_id, StableIterator<SimMutex> mutex, UInt64 time)
               : _core_id(core_id), _mutex(mutex), _arrival_time(time) {}
         core_id_t _core_id;
         StableIterator<SimMutex> _mutex;
         UInt64 _arrival_time;
   };

   typedef std::vector< CondWaiter > ThreadQueue;
   ThreadQueue _waiting;
};

class SimBarrier
{
public:
   typedef std::vector<core_id_t> WakeupList;

   SimBarrier(UInt32 count);
   ~SimBarrier();

   // returns a list of threads to wake up if all have reached barrier
   void wait(UInt64 time, core_id_t core_id, WakeupList &woken);
   UInt64 getMaxTime() { return _max_time; }

private:
   typedef std::vector< core_id_t > ThreadQueue;
   ThreadQueue _waiting;

   UInt32 _count;
   UInt64 _max_time;
};

class SyncManager
{
private:
   typedef std::vector<SimMutex> MutexVector;
   typedef std::vector<SimCond> CondVector;
   typedef std::vector<SimBarrier> BarrierVector;

   Lock _lock;
   MutexVector _mutexes;
   CondVector _conds;
   BarrierVector _barriers;

public:
   SyncManager();
   ~SyncManager();

   // Remaining parameters to these functions are stored
   // in the recv buffer and get unpacked
   void mutexInit(UInt64 time, core_id_t core_id, carbon_mutex_t* mux_ptr);
   void mutexLock(UInt64 time, core_id_t core_id, carbon_mutex_t* mux_ptr);
   void mutexUnlock(UInt64 time, core_id_t core_id, carbon_mutex_t* mux_ptr);

   void condInit(UInt64 time, core_id_t core_id, carbon_cond_t* cond_ptr);
   void condWait(UInt64 time, core_id_t core_id, carbon_cond_t* cond_ptr, carbon_mutex_t* mux_ptr);
   void condSignal(UInt64 time, core_id_t core_id, carbon_cond_t* cond_ptr);
   void condBroadcast(UInt64 time, core_id_t core_id, carbon_cond_t* cond_ptr);

   void barrierInit(UInt64 time, core_id_t core_id, carbon_barrier_t* barrier_ptr, UInt32 count);
   void barrierWait(UInt64 time, core_id_t core_id, carbon_barrier_t* barrier_ptr);
};
