#pragma once

#include <pthread.h>

#include "pin.H"
#include "carbon_user.h"
#include "fixed_types.h"

namespace CycleAccurate 
{

enum RoutineId
{
   // Init/Enable/Disable/Reset models from application
   CARBON_INIT_MODELS = 0,
   CARBON_ENABLE_MODELS,
   CARBON_DISABLE_MODELS,
   CARBON_RESET_MODELS,

   // Spawn/Join Threads
   EMULATE_CARBON_SPAWN_THREAD,
   EMULATE_PTHREAD_CREATE,
   EMULATE_CARBON_JOIN_THREAD,
   EMULATE_PTHREAD_JOIN,

   // Synchronization primitives
   CARBON_MUTEX_INIT,
   CARBON_MUTEX_LOCK,
   CARBON_MUTEX_UNLOCK,
   CARBON_COND_INIT,
   CARBON_COND_WAIT,
   CARBON_COND_SIGNAL,
   CARBON_COND_BROADCAST,
   CARBON_BARRIER_INIT,
   CARBON_BARRIER_WAIT,

   // CAPI Functions
   CAPI_INITIALIZE,
   CAPI_RANK,
   CAPI_MESSAGE_SEND_W,
   CAPI_MESSAGE_RECEIVE_W,
   CAPI_MESSAGE_SEND_W_EX,
   CAPI_MESSAGE_RECEIVE_W_EX,

   // Get Time
   CARBON_GET_TIME,

   // Get/Set Frequency
   CARBON_GET_CORE_FREQUENCY,
   CARBON_SET_CORE_FREQUENCY,

   // Special Routines inside simulator
   ENABLE_PERFORMANCE_MODELS,
   DISABLE_PERFORMANCE_MODELS,

   // Null Routine
   NULL_FUNCTION,

   NUM_ROUTINES
};

__attribute((__unused__)) static UInt32 _num_routine_args[] = {
   
   // Init/Enable/Disable/Reset models from application
   0, // CARBON_INIT_MODELS,
   0, // CARBON_ENABLE_MODELS,
   0, // CARBON_DISABLE_MODELS,
   0, // CARBON_RESET_MODELS,
   
   // Spawn/Join Threads
   3, // EMULATE_CARBON_SPAWN_THREAD,
   5, // EMULATE_PTHREAD_CREATE,
   2, // EMULATE_CARBON_JOIN_THREAD,
   3, // EMULATE_PTHREAD_JOIN,
   
   // Synchronization primitives
   1, // CARBON_MUTEX_INIT,
   1, // CARBON_MUTEX_LOCK,
   1, // CARBON_MUTEX_UNLOCK,
   1, // CARBON_COND_INIT,
   2, // CARBON_COND_WAIT,
   1, // CARBON_COND_SIGNAL,
   1, // CARBON_COND_BROADCAST,
   2, // CARBON_BARRIER_INIT,
   1, // CARBON_BARRIER_WAIT,
   
   // CAPI Functions
   1, // CAPI_INITIALIZE,
   1, // CAPI_RANK,
   4, // CAPI_MESSAGE_SEND_W,
   4, // CAPI_MESSAGE_RECEIVE_W,
   5, // CAPI_MESSAGE_SEND_W_EX,
   5, // CAPI_MESSAGE_RECEIVE_W_EX,
   
   // Get Time
   0, // CARBON_GET_TIME,
   
   // Get/Set Frequency
   1, // CARBON_GET_CORE_FREQUENCY,
   1, // CARBON_SET_CORE_FREQUENCY,
   
   // Special Routines inside simulator
   0, // ENABLE_PERFORMANCE_MODELS,
   0, // DISABLE_PERFORMANCE_MODELS,
   
   // Null Routine
   0 // NULL_FUNCTION,
};

void routineCallback(RTN rtn, void* v);

carbon_thread_t emuCarbonSpawnThread(CONTEXT* context, thread_func_t thread_func, void* arg);
int emuPthreadCreate(CONTEXT* context, pthread_t* thread_ptr, pthread_attr_t* attr, thread_func_t thread_func, void* arg);
void emuCarbonJoinThread(CONTEXT* context, carbon_thread_t tid);
int emuPthreadJoin(CONTEXT* context, pthread_t thread, void** thead_return);
IntPtr nullFunction();

AFUNPTR getFunptr(CONTEXT* context, string func_name);

// IntPtr executeRoutine(UInt32 routine_id, ...);

}
