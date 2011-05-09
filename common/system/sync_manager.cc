#include "sync_manager.h"
#include "simulator.h"
#include "thread_manager.h"

using namespace std;

// -- SimMutex -- //

SimMutex::SimMutex()
      : _owner(NO_OWNER)
{}

SimMutex::~SimMutex()
{
   assert(_waiting.empty());
}

bool
SimMutex::lock(core_id_t core_id)
{
   if (_owner == NO_OWNER)
   {
      _owner = core_id;
      return true;
   }
   else
   {
      _waiting.push(core_id);
      return false;
   }
}

core_id_t
SimMutex::unlock(core_id_t core_id)
{
   assert(_owner == core_id);
   if (_waiting.empty())
   {
      _owner = NO_OWNER;
   }
   else
   {
      _owner =  _waiting.front();
      _waiting.pop();
   }
   return _owner;
}

// -- SimCond -- //
// FIXME: Currently, 'simulated times' are ignored in the synchronization constructs
SimCond::SimCond()
{}

SimCond::~SimCond()
{
   assert(_waiting.empty());
}

core_id_t
SimCond::wait(UInt64 time, core_id_t core_id, StableIterator<SimMutex> & simMux)
{
   // If we don't have any later signals, then put this request in the queue
   _waiting.push_back(CondWaiter(core_id, simMux, time));
   return simMux->unlock(core_id);
}

core_id_t
SimCond::signal(UInt64 time, core_id_t core_id)
{
   // If there is a list of threads waiting, wake up one of them
   if (!_waiting.empty())
   {
      CondWaiter woken = *(_waiting.begin());
      _waiting.erase(_waiting.begin());

      if (woken._mutex->lock(woken._core_id))
      {
         // Woken up thread is able to grab lock immediately
         return woken._core_id;
      }
      else
      {
         // Woken up thread is *NOT* able to grab lock immediately
         return INVALID_CORE_ID;
      }
   }

   // There are *NO* threads waiting on the condition variable
   return INVALID_CORE_ID;
}

void
SimCond::broadcast(UInt64 time, core_id_t core_id, WakeupList &woken_list)
{
   for (ThreadQueue::iterator i = _waiting.begin(); i != _waiting.end(); i++)
   {
      CondWaiter woken = *(i);

      if (woken._mutex->lock(woken._core_id))
      {
         // Woken up thread is able to grab lock immediately
         woken_list.push_back(woken._core_id);
      }
   }

   // All waiting threads have been woken up from the CondVar queue
   _waiting.clear();
}

// -- SimBarrier -- //
SimBarrier::SimBarrier(UInt32 count)
      : _count(count)
      , _max_time(0)
{
}

SimBarrier::~SimBarrier()
{
   assert(_waiting.empty());
}

void
SimBarrier::wait(UInt64 time, core_id_t core_id, WakeupList &woken_list)
{
   _waiting.push_back(core_id);

   assert(_waiting.size() <= _count);

   if (_waiting.size() > 1)
      assert(time >= _max_time);

   _max_time = time;

   // All threads have reached the barrier
   if (_waiting.size() == _count)
   {
      woken_list = _waiting;
      _waiting.clear();
   }
}

// -- SyncManager -- //

SyncManager::SyncManager()
{}

SyncManager::~SyncManager()
{}

void
SyncManager::mutexInit(UInt64 time, core_id_t core_id, carbon_mutex_t* mux_ptr)
{
   ScopedLock sl(_lock);

   _mutexes.push_back(SimMutex());
   UInt32 mux = (UInt32)_mutexes.size()-1;

   *mux_ptr = mux;
   // Alert the init core
   Sim()->getThreadInterface(core_id)->sendSimReply(time);
}

void
SyncManager::mutexLock(UInt64 time, core_id_t core_id, carbon_mutex_t* mux_ptr)
{
   ScopedLock sl(_lock);

   carbon_mutex_t mux = *mux_ptr;
   assert((size_t)mux < _mutexes.size());

   SimMutex *psimmux = &_mutexes[mux];

   if (psimmux->lock(core_id))
   {
      // notify the owner
      Sim()->getThreadInterface(core_id)->sendSimReply(time);
   }
   else
   {
      // nothing...thread goes to sleep
   }
}

void
SyncManager::mutexUnlock(UInt64 time, core_id_t core_id, carbon_mutex_t* mux_ptr)
{
   ScopedLock sl(_lock);

   carbon_mutex_t mux = *mux_ptr;
   assert((size_t)mux < _mutexes.size());

   SimMutex *psimmux = &_mutexes[mux];

   core_id_t new_owner = psimmux->unlock(core_id);

   if (new_owner != SimMutex::NO_OWNER)
   {
      // wake up the new owner
      Sim()->getThreadInterface(new_owner)->sendSimReply(time);
   }
   else
   {
      // nothing...
   }

   // Alert the unlocker
   Sim()->getThreadInterface(core_id)->sendSimReply(time);
}

// -- Condition Variable Stuffs -- //
void
SyncManager::condInit(UInt64 time, core_id_t core_id, carbon_cond_t* cond_ptr)
{
   ScopedLock sl(_lock);

   _conds.push_back(SimCond());
   UInt32 cond = (UInt32)_conds.size()-1;

   *cond_ptr = cond;
   // alert the initializer
   Sim()->getThreadInterface(core_id)->sendSimReply(time);
}

void
SyncManager::condWait(UInt64 time, core_id_t core_id, carbon_cond_t* cond_ptr, carbon_mutex_t* mux_ptr)
{
   ScopedLock sl(_lock);

   carbon_cond_t cond = *cond_ptr;
   carbon_mutex_t mux = *mux_ptr;

   assert((size_t)mux < _mutexes.size());
   assert((size_t)cond < _conds.size());

   SimCond *psimcond = &_conds[cond];

   StableIterator<SimMutex> it(_mutexes, mux);
   core_id_t new_mutex_owner = psimcond->wait(time, core_id, it);

   if (new_mutex_owner != SimMutex::NO_OWNER)
   {
      // wake up the new owner
      Sim()->getThreadInterface(new_mutex_owner)->sendSimReply(time);
   }
}


void
SyncManager::condSignal(UInt64 time, core_id_t core_id, carbon_cond_t* cond_ptr)
{
   ScopedLock sl(_lock);

   carbon_cond_t cond = *cond_ptr;
   assert((size_t)cond < _conds.size());

   SimCond *psimcond = &_conds[cond];

   core_id_t woken = psimcond->signal(time, core_id);

   if (woken != INVALID_CORE_ID)
   {
      // wake up the new owner
      Sim()->getThreadInterface(woken)->sendSimReply(time);
   }
   else
   {
      // nothing...
   }

   // Alert the signaler
   Sim()->getThreadInterface(core_id)->sendSimReply(time);
}

void
SyncManager::condBroadcast(UInt64 time, core_id_t core_id, carbon_cond_t* cond_ptr)
{
   ScopedLock sl(_lock);

   carbon_cond_t cond = *cond_ptr;
   assert((size_t)cond < _conds.size());

   SimCond *psimcond = &_conds[cond];

   SimCond::WakeupList woken_list;
   psimcond->broadcast(time, core_id, woken_list);

   for (SimCond::WakeupList::iterator it = woken_list.begin(); it != woken_list.end(); it++)
   {
      assert(*it != INVALID_CORE_ID);

      // wake up the new owner
      Sim()->getThreadInterface(*it)->sendSimReply(time);
   }

   // Alert the signaler
   Sim()->getThreadInterface(core_id)->sendSimReply(time);
}

void
SyncManager::barrierInit(UInt64 time, core_id_t core_id, carbon_barrier_t* barrier_ptr, UInt32 count)
{
   ScopedLock sl(_lock);

   _barriers.push_back(SimBarrier(count));
   UInt32 barrier = (UInt32)_barriers.size()-1;

   *barrier_ptr = barrier;
   Sim()->getThreadInterface(core_id)->sendSimReply(time);
}

void
SyncManager::barrierWait(UInt64 time, core_id_t core_id, carbon_barrier_t* barrier_ptr)
{
   ScopedLock sl(_lock);

   LOG_PRINT("barrierWait(Time[%llu], CoreID[%i], Barrier[%i])", time, core_id, *barrier_ptr);

   carbon_barrier_t barrier = *barrier_ptr;
   assert((size_t)barrier < _barriers.size());

   SimBarrier *psimbarrier = &_barriers[barrier];
   LOG_PRINT("psimbarrier(%p)", psimbarrier);

   SimBarrier::WakeupList woken_list;
   psimbarrier->wait(time, core_id, woken_list);

   UInt64 max_time = psimbarrier->getMaxTime();

   for (SimBarrier::WakeupList::iterator it = woken_list.begin(); it != woken_list.end(); it++)
   {
      LOG_PRINT("Waking Up(%i)", *it);
      assert(*it != INVALID_CORE_ID);
      // Release the barrier - Notify the waiters
      Sim()->getThreadInterface(*it)->sendSimReply(max_time);
      LOG_PRINT("Sent Reply and Signaled the semaphore: Time(%llu)", max_time);
   }
}
