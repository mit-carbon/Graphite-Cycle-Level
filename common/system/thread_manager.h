#pragma once

#include <vector>
using std::vector;

#include "core.h"
#include "fixed_types.h"
#include "lock.h"
#include "semaphore.h"
#include "thread_support.h"

class CoreManager;

class ThreadManager
{
public:
   ThreadManager(CoreManager*);
   ~ThreadManager();

   // services
   void spawnThread(UInt64 time, core_id_t req_core_id, thread_func_t func, void* arg);
   void joinThread(UInt64 time, core_id_t req_core_id, core_id_t join_core_id);

   // events
   void onThreadStart(core_id_t core_id);
   void onThreadExit();
   void onThreadExit(core_id_t core_id);

   // Thread that is currently being spawn
   ThreadSpawnRequest dequeueThreadSpawnReq();
   
   // Core Id -> pthread_t* mapping
   void insertCoreIDToThreadMapping(core_id_t core_id, pthread_t* thread);
   void eraseCoreIDToThreadMapping(core_id_t core_id, pthread_t* thread);
   pthread_t* getThreadFromCoreID(core_id_t core_id);
   core_id_t getCoreIDFromThread(pthread_t* thread);

private:
   core_id_t getFreeCoreID();
   void wakeUpWaiter(UInt64 time, core_id_t core_id);
   void checkLegalCoreId(core_id_t core_id);

   struct CoreState
   {
      CoreState(): _status(Core::IDLE), _waiter(INVALID_CORE_ID) {}
      Core::Status _status;
      core_id_t _waiter;
   };

   vector<CoreState> _core_state;
   Lock _lock;
   Semaphore _semaphore;

   CoreManager* _core_manager;

   vector<list<pthread_t*> > _core_id__to__thread_list__mapping;
   ThreadSpawnRequest _curr_thread_spawn_req;

   void setCurrThreadSpawnRequest(core_id_t core_id, thread_func_t func, void* arg);
};
