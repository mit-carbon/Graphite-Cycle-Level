#include "simulator.h"
#include "sync_manager.h"
#include "routine_manager.h"
#include "packetize.h"

void CarbonMutexInit(carbon_mutex_t* mux)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_MUTEX_INIT << mux;
   emulateRoutine(routine_info);
}

void __CarbonMutexInit(UInt64 time, core_id_t core_id, carbon_mutex_t* mux)
{
   Sim()->getSyncManager()->mutexInit(time, core_id, mux);
}

void CarbonMutexLock(carbon_mutex_t *mux)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_MUTEX_LOCK << mux;
   emulateRoutine(routine_info);
}

void __CarbonMutexLock(UInt64 time, core_id_t core_id, carbon_mutex_t* mux)
{
   Sim()->getSyncManager()->mutexLock(time, core_id, mux);
}

void CarbonMutexUnlock(carbon_mutex_t *mux)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_MUTEX_UNLOCK << mux;
   emulateRoutine(routine_info);
}

void __CarbonMutexUnlock(UInt64 time, core_id_t core_id, carbon_mutex_t* mux)
{
   Sim()->getSyncManager()->mutexUnlock(time, core_id, mux);
}

void CarbonCondInit(carbon_cond_t* cond)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_COND_INIT << cond;
   emulateRoutine(routine_info);
}

void __CarbonCondInit(UInt64 time, core_id_t core_id, carbon_cond_t* cond)
{
   Sim()->getSyncManager()->condInit(time, core_id, cond);
}

void CarbonCondWait(carbon_cond_t* cond, carbon_mutex_t* mux)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_COND_WAIT << cond << mux;
   emulateRoutine(routine_info);
}

void __CarbonCondWait(UInt64 time, core_id_t core_id, carbon_cond_t* cond, carbon_mutex_t* mux)
{
   Sim()->getSyncManager()->condWait(time, core_id, cond, mux);
}

void CarbonCondSignal(carbon_cond_t* cond)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_COND_SIGNAL << cond;
   emulateRoutine(routine_info);
}

void __CarbonCondSignal(UInt64 time, core_id_t core_id, carbon_cond_t* cond)
{
   Sim()->getSyncManager()->condSignal(time, core_id, cond);
}

void CarbonCondBroadcast(carbon_cond_t* cond)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_COND_BROADCAST << cond;
   emulateRoutine(routine_info);
}

void __CarbonCondBroadcast(UInt64 time, core_id_t core_id, carbon_cond_t* cond)
{
   Sim()->getSyncManager()->condBroadcast(time, core_id, cond);
}

void CarbonBarrierInit(carbon_barrier_t* barrier, UInt32 count)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_BARRIER_INIT << barrier << count;
   emulateRoutine(routine_info);
}

void __CarbonBarrierInit(UInt64 time, core_id_t core_id, carbon_barrier_t* barrier, UInt32 count)
{
   Sim()->getSyncManager()->barrierInit(time, core_id, barrier, count);
}

void CarbonBarrierWait(carbon_barrier_t* barrier)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_BARRIER_WAIT << barrier;
   emulateRoutine(routine_info);
}

void __CarbonBarrierWait(UInt64 time, core_id_t core_id, carbon_barrier_t* barrier)
{
   Sim()->getSyncManager()->barrierWait(time, core_id, barrier);
}
