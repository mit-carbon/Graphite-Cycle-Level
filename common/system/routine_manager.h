#pragma once

#include "fixed_types.h"
#include "packetize.h"

class Routine
{
public:
   enum Id
   {
      // Thread Management
      CARBON_SPAWN_THREAD = 0,
      CARBON_JOIN_THREAD,
      // Synchronization
      CARBON_MUTEX_INIT,
      CARBON_MUTEX_LOCK,
      CARBON_MUTEX_UNLOCK,
      CARBON_COND_INIT,
      CARBON_COND_WAIT,
      CARBON_COND_SIGNAL,
      CARBON_COND_BROADCAST,
      CARBON_BARRIER_INIT,
      CARBON_BARRIER_WAIT,
      // CAPI
      CAPI_INITIALIZE,
      CAPI_RANK,
      CAPI_MESSAGE_SEND,
      CAPI_MESSAGE_SEND_EXPLICIT,
      CAPI_MESSAGE_RECEIVE,
      CAPI_MESSAGE_RECEIVE_EXPLICIT,
      // Get Time
      CARBON_GET_TIME,
      // DVFS
      CARBON_GET_CORE_FREQUENCY,
      CARBON_SET_CORE_FREQUENCY,
      // Enable/Disable Models
      ENABLE_PERFORMANCE_MODELS,
      DISABLE_PERFORMANCE_MODELS,
      NUM_ROUTINES
   };
};

// Called from App thread
IntPtr emulateRoutine(Routine::Id routine_id);
IntPtr emulateRoutine(UnstructuredBuffer* routine_info);
// Called from Sim thread
void __emulateRoutine(UInt64 time, core_id_t core_id, Routine::Id routine_id, UnstructuredBuffer* routine_args);
