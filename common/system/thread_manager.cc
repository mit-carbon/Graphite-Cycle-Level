#include "thread_manager.h"
#include "core_manager.h"
#include "config.h"
#include "log.h"
#include "simulator.h"
#include "core.h"
#include "packetize.h"
#include "clock_converter.h"
#include "event.h"

ThreadManager::ThreadManager(CoreManager *core_manager)
   : _core_manager(core_manager)
{
   _core_state.resize(Config::getSingleton()->getTotalCores());
   _core_state[0]._status = Core::RUNNING;

   // Core ID -> pthread_t* mapping
   _core_id__to__thread_list__mapping.resize(Config::getSingleton()->getTotalCores());
   setCurrThreadSpawnRequest(INVALID_CORE_ID, NULL, NULL);
}

ThreadManager::~ThreadManager()
{
   // Check that there are no transient thread spawn requests
   // and all threads have been joined
   assert(_curr_thread_spawn_req.core_id == INVALID_CORE_ID);
   for (UInt32 i = 0; i < Config::getSingleton()->getTotalCores(); i++)
      assert(_core_id__to__thread_list__mapping[i].empty());

   // Check that the states of all cores are idle
   for (UInt32 i = 0; i < _core_state.size(); i++)
      LOG_ASSERT_ERROR(_core_state[i]._status == Core::IDLE,
            "Core %u still active when ThreadManager destructs!", i);
}

void
ThreadManager::onThreadStart(core_id_t core_id)
{
   // Called by App Thread
   
   // Acquire Thread Control Lock
   ScopedLock sl(_lock);

   assert(_core_state[core_id]._status == Core::RUNNING);

   LOG_PRINT("Initializing Thread to Core(%i)", core_id);
   // Initialize this thread
   _core_manager->initializeThread(core_id);

   LOG_PRINT("Initialized Thread to Core(%i)", core_id);

   // Initiate a service request to tell the sim thread to start processing on this core
   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) << core_id;
   EventStartThread* event = new EventStartThread(0, event_args);
   Event::processInOrder(event, core_id, EventQueue::ORDERED);
   LOG_PRINT("Pushed START_THREAD event");
  
   // Tell Thread Spawner to proceed
   _semaphore.signal();
}

void
ThreadManager::onThreadExit()
{
   // Called by App Thread

   // Get the core that is run on
   Core* core = _core_manager->getCurrentCore();

   // terminate thread locally so we are ready for new thread requests
   // on that core
   _core_manager->terminateThread();

   // Send request to sim thread for rest of processing
   AppRequest app_request(AppRequest::PROCESS_THREAD_EXIT);
   Sim()->getThreadInterface(core->getId())->sendAppRequest(app_request);
}

void
ThreadManager::onThreadExit(core_id_t core_id)
{
   // Called by Sim Thread

   // Acquire Thread Control Lock
   ScopedLock sl(_lock);

   // Recompute Average Frequency
   Core* core = _core_manager->getCoreFromID(core_id);
   PerformanceModel* pm = core->getPerformanceModel();
   pm->recomputeAverageFrequency();
   UInt64 time = pm->getTime();
   
   // update global core state
   _core_state[core_id]._status = Core::IDLE;

   // Wake up any waiters
   wakeUpWaiter(time, core_id);
}

ThreadSpawnRequest
ThreadManager::dequeueThreadSpawnReq()
{
   ScopedLock sl(_lock);

   LOG_PRINT("getThreadSpawnReq(CoreID[%i],func[%p],arg[%p])",
         _curr_thread_spawn_req.core_id, _curr_thread_spawn_req.func, _curr_thread_spawn_req.arg);
   ThreadSpawnRequest req_clone = _curr_thread_spawn_req;
   setCurrThreadSpawnRequest(INVALID_CORE_ID, NULL, NULL);

   return req_clone;
}

void
ThreadManager::setCurrThreadSpawnRequest(core_id_t core_id, thread_func_t func, void* arg)
{
   LOG_PRINT("setCurrThreadSpawnRequest(CoreID[%i],func[%p],arg[%p])", core_id, func, arg);
   _curr_thread_spawn_req.core_id = core_id;
   _curr_thread_spawn_req.func = func;
   _curr_thread_spawn_req.arg = arg;
}

void
ThreadManager::spawnThread(UInt64 time, core_id_t req_core_id, thread_func_t func, void* arg)
{
   // Called from SIM thread
   _semaphore.wait();

   // Acquire Thread Control Lock
   ScopedLock sl(_lock);

   LOG_PRINT("spawnThread(Time[%llu], req_core_id[%i], func[%p], arg[%p])",
         (long long unsigned int) time, req_core_id, func, arg);

   // Check that no other thread is being spawned now
   LOG_ASSERT_ERROR(_curr_thread_spawn_req.core_id == INVALID_CORE_ID,
         "Two threads cannot be spawned simultaneously");

   // Get a free core and set the core on which a thread is being spawned
   core_id_t spawned_core_id = getFreeCoreID();
   setCurrThreadSpawnRequest(spawned_core_id, func, arg);
  
   // Set the status of the core being spawned 
   _core_state[spawned_core_id]._status = Core::RUNNING;

   // Send Reply to App Thread
   Sim()->getThreadInterface(req_core_id)->sendSimReply(time, (IntPtr) spawned_core_id);
}

void
ThreadManager::joinThread(UInt64 time, core_id_t req_core_id, core_id_t join_core_id)
{
   // Called from SIM thread

   // Acquire Thread Control Lock
   ScopedLock sl(_lock);

   _core_state[join_core_id]._waiter = req_core_id;
   
   if (_core_state[join_core_id]._status == Core::IDLE)
   {
      // Tell App Thread to proceed - This is a magic JOIN
      wakeUpWaiter(time+1, join_core_id);
   }
}

core_id_t
ThreadManager::getFreeCoreID()
{
   // Find Core Id to use
   // FIXME: Load balancing?
   core_id_t core_id = INVALID_CORE_ID;
   for (SInt32 i = 0; i < (SInt32) _core_state.size(); i++)
   {
      if (_core_state[i]._status == Core::IDLE)
      {
         core_id = i;
         break;
      }
   }
   LOG_ASSERT_ERROR(core_id != INVALID_CORE_ID, "No cores available for spawnThread request.");
   return core_id;
}

void
ThreadManager::wakeUpWaiter(UInt64 time, core_id_t core_id)
{
   assert(_core_state[core_id]._status == Core::IDLE);
   core_id_t waiter = _core_state[core_id]._waiter;

   if (waiter != INVALID_CORE_ID)
   {
      // Tell the waiter to proceed
      Sim()->getThreadInterface(waiter)->sendSimReply(time + 1);
      // Now, no-one is waiting for me
      _core_state[core_id]._waiter = INVALID_CORE_ID;
   }
}

void
ThreadManager::checkLegalCoreId(core_id_t core_id)
{
   LOG_ASSERT_ERROR(0 <= core_id && core_id < (core_id_t) _core_state.size(),
         "Core Id(%i), Limits(0,%u)", core_id, _core_state.size());
}

void
ThreadManager::insertCoreIDToThreadMapping(core_id_t core_id, pthread_t* thread)
{
   _core_id__to__thread_list__mapping[core_id].push_back(thread);
}

void
ThreadManager::eraseCoreIDToThreadMapping(core_id_t core_id, pthread_t* thread)
{
   list<pthread_t*>& thread_list = _core_id__to__thread_list__mapping[core_id];
   list<pthread_t*>::iterator it = thread_list.begin();
   for ( ; it != thread_list.end(); it++)
   {
      if (pthread_equal(*thread, *(*it)))
      {
         thread_list.erase(it);
         break;
      }
   }
}

pthread_t*
ThreadManager::getThreadFromCoreID(core_id_t core_id)
{
   return _core_id__to__thread_list__mapping[core_id].front();
}

core_id_t
ThreadManager::getCoreIDFromThread(pthread_t* thread)
{
   for (UInt32 i = 0; i < Config::getSingleton()->getTotalCores(); i++)
   {
      list<pthread_t*>& thread_list = _core_id__to__thread_list__mapping[i];
      list<pthread_t*>::iterator it = thread_list.begin();
      for ( ; it != thread_list.end(); it++)
      {
         if (pthread_equal(*thread, *(*it)) != 0)
            return i;
      }
   }
   return INVALID_CORE_ID;
}
