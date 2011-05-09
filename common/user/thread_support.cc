#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

#include "simulator.h"
#include "thread_manager.h"
#include "core_manager.h"
#include "core.h"
#include "config_file.hpp"
#include "routine_manager.h"
#include "carbon_user.h"
#include "thread_support_private.h"

core_id_t CarbonSpawnThread(thread_func_t func, void *arg)
{
   LOG_PRINT("CarbonSpawnThread(func[%p],arg[%p]) enter", func, arg);
   // Emulate CARBON_SPAWN_THREAD Routine
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_SPAWN_THREAD << func << arg;
   core_id_t spawned_core_id = (core_id_t) emulateRoutine(routine_info);

   // pthread_create
   pthread_t* thread_ptr = new pthread_t;
   CarbonPthreadCreate(thread_ptr);

   // Insert <core_id, thread_t*> mapping 
   Sim()->getThreadManager()->insertCoreIDToThreadMapping(spawned_core_id, thread_ptr);
   LOG_PRINT("Inserted Mapping(CoreID[%i], ThreadPtr[%p])", spawned_core_id, thread_ptr);
   
   LOG_PRINT("CarbonSpawnThread(func[%p],arg[%p]) exit", func, arg);
   return spawned_core_id;
}

void CarbonJoinThread(core_id_t join_core_id)
{
   LOG_PRINT("CarbonJoinThread(CoreID[%i]) enter", join_core_id);

   // Look up thread_t* from core_id
   pthread_t* thread_ptr = Sim()->getThreadManager()->getThreadFromCoreID(join_core_id);
   assert(thread_ptr);
   // Erase the Core Id to pthread_t* mapping
   LOG_PRINT("Erasing Mapping(CoreID(%i), ThreadPtr[%p])", join_core_id, thread_ptr);
   Sim()->getThreadManager()->eraseCoreIDToThreadMapping(join_core_id, thread_ptr);
   LOG_PRINT("Erased Mapping(CoreID(%i), ThreadPtr[%p])", join_core_id, thread_ptr);

   // Emulate CARBON_JOIN_THREAD Routine
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_JOIN_THREAD << join_core_id;
   emulateRoutine(routine_info); 
  
   // pthread_join 
   int ret = pthread_join(*thread_ptr, NULL);
   assert(ret == 0);
   delete thread_ptr;
   
   LOG_PRINT("CarbonJoinThread(CoreID[%i]) exit", join_core_id);
}

void __CarbonSpawnThread(UInt64 time, core_id_t req_core_id, thread_func_t func, void* arg)
{
   Sim()->getThreadManager()->spawnThread(time, req_core_id, func, arg);
}

void __CarbonJoinThread(UInt64 time, core_id_t req_core_id, core_id_t join_core_id)
{
   Sim()->getThreadManager()->joinThread(time, req_core_id, join_core_id);
}

// Support functions provided by the simulator
void CarbonThreadStart(core_id_t core_id)
{
   Sim()->getThreadManager()->onThreadStart(core_id);
}

void CarbonThreadExit()
{
   Sim()->getThreadManager()->onThreadExit();
}

ThreadSpawnRequest CarbonDequeueThreadSpawnReq()
{
   return Sim()->getThreadManager()->dequeueThreadSpawnReq();
}

void* CarbonManagedThread(void*)
{
   LOG_PRINT("In Spawned Thread");

   ThreadSpawnRequest req = CarbonDequeueThreadSpawnReq();

   CarbonThreadStart(req.core_id);

   req.func(req.arg);

   CarbonThreadExit();
   
   return NULL;
}

void CarbonPthreadCreate(pthread_t* thread)
{
   // Set other attributes   
   // pthread_attr_t attr;
   // pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
   // CarbonPthreadAttrInitOtherAttr(&attr);

   LOG_PRINT("Starting pthread_create()");
   int ret = pthread_create(thread, NULL, CarbonManagedThread, NULL);
   assert(ret == 0);
   LOG_PRINT("Finished pthread_create()");
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
