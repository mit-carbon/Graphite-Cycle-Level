#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sched.h>
#include <pthread.h>
#include <map>
using std::multimap;

#include "simulator.h"
#include "thread_manager.h"
#include "core_manager.h"
#include "core.h"
#include "config_file.hpp"
#include "carbon_user.h"
#include "thread_support_private.h"

// FIXME: Pthread wrappers are untested.
int CarbonPthreadCreate(pthread_t *tid, int *attr, thread_func_t func, void *arg)
{
   LOG_ASSERT_WARNING(attr == NULL, "Attributes ignored in pthread_create.");
   LOG_ASSERT_ERROR(tid != NULL, "Null pointer passed to pthread_create.");

   *tid = CarbonSpawnThread(func, arg);
   return *tid >= 0 ? 0 : 1;
}

int CarbonPthreadJoin(pthread_t tid, void **pparg)
{
   LOG_ASSERT_WARNING(pparg == NULL, "Did not expect pparg non-NULL. It is ignored.");
   CarbonJoinThread(tid);
   return 0;
}

// Global Map
multimap<core_id_t, pthread_t*> _tid_to_thread_ptr_map;

carbon_thread_t CarbonSpawnThread(thread_func_t func, void *arg)
{
   // FIXME: Put a lock here since multiple threads can call CarbonSpawnThread simultaneously  
   LOG_PRINT("Sim()->getThreadManager()->spawnThread(%p, %p) start", func, arg);
   core_id_t tid = Sim()->getThreadManager()->spawnThread(func, arg);
   LOG_PRINT("Sim()->getThreadManager()->spawnThread(%p, %p) end", func, arg);
   
   LOG_PRINT("CarbonEmulatePthreadCreate() start");
   pthread_t* thread_ptr = new pthread_t;
   bool spawned = CarbonEmulatePthreadCreate(thread_ptr);
   assert(spawned);
   LOG_PRINT("pthread_create end");
 
   _tid_to_thread_ptr_map.insert(make_pair<core_id_t, pthread_t*>(tid, thread_ptr));
   return tid;
}

void CarbonJoinThread(carbon_thread_t tid)
{
   // FIXME: Put a lock here since multiple threads can call CarbonJoinThread simultaneously  
   multimap<core_id_t,pthread_t*>::iterator it = _tid_to_thread_ptr_map.find(tid);
   LOG_ASSERT_ERROR(it != _tid_to_thread_ptr_map.end(), "Cant find thread_ptr for tid(%i)", tid);
   pthread_t* thread_ptr = it->second;
   _tid_to_thread_ptr_map.erase(it);
   
   LOG_PRINT("Sim()->getThreadManager()->joinThread(%i) start", tid);
   Sim()->getThreadManager()->joinThread(tid);
   LOG_PRINT("Sim()->getThreadManager()->joinThread(%i) end", tid);
   
   LOG_PRINT("pthread_join start");
   int ret = pthread_join(*thread_ptr, NULL);
   delete thread_ptr;
   assert(ret == 0);
   LOG_PRINT("pthread_join end");
}

// Support functions provided by the simulator
void CarbonThreadStart(ThreadSpawnRequest *req)
{
   Sim()->getThreadManager()->onThreadStart(req);
}

void CarbonThreadExit()
{
   Sim()->getThreadManager()->onThreadExit();
}

void CarbonGetThreadToSpawn(ThreadSpawnRequest *req)
{
   Sim()->getThreadManager()->getThreadToSpawn(req);
}

void CarbonDequeueThreadSpawnReq (ThreadSpawnRequest *req)
{
   Sim()->getThreadManager()->dequeueThreadSpawnReq (req);
}


void *CarbonSpawnManagedThread(void *)
{
   ThreadSpawnRequest thread_req;

   CarbonDequeueThreadSpawnReq (&thread_req);

   CarbonThreadStart(&thread_req);

   thread_req.func(thread_req.arg);

   CarbonThreadExit();
   
   return NULL;
}

// This function spawns the thread spawner
int CarbonSpawnThreadSpawner()
{
   setvbuf(stdout, NULL, _IONBF, 0 );

   pthread_t thread;
   pthread_attr_t attr;
   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

   CarbonPthreadAttrInitOtherAttr(&attr);
       
   pthread_create(&thread, &attr, CarbonThreadSpawner, NULL);

   return 0;
}

// This function will spawn threads provided by the sim
void *CarbonThreadSpawner(void *)
{
   ThreadSpawnRequest req = {-1, NULL, NULL, -1, Sim()->getConfig()->getCurrentThreadSpawnerCoreNum() };

   CarbonThreadStart (&req);

   while(1)
   {
      pthread_t thread;
      bool spawned = CarbonEmulatePthreadCreate(&thread);
      if (!spawned)
         break;
   }

   CarbonThreadExit();

   return NULL;
}

bool CarbonEmulatePthreadCreate(pthread_t* thread)
{
   ThreadSpawnRequest req;
      
   // Wait for a spawn request
   CarbonGetThreadToSpawn(&req);

   if(req.func)
   {
      pthread_attr_t attr;
      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
     
      CarbonPthreadAttrInitOtherAttr(&attr);

      pthread_create(thread, &attr, CarbonSpawnManagedThread, NULL);
      return true;
   }
   return false;
}

// This function initialized the pthread attributes
// Gets replaced while running with Pin
// attribute 'noinline' necessary to make the scheme work correctly with
// optimizations enabled; asm ("") in the body prevents the function from being
// optimized away
__attribute__((noinline)) void CarbonPthreadAttrInitOtherAttr(pthread_attr_t *attr)
{
   asm ("");
}
