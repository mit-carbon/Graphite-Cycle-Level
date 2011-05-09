#ifndef SYNC_API_H
#define SYNC_API_H
#include <stdbool.h>

#include "fixed_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef SInt32 carbon_mutex_t;
typedef SInt32 carbon_cond_t;
typedef SInt32 carbon_barrier_t;

// Related to Mutexes
void CarbonMutexInit(carbon_mutex_t *mux);
void CarbonMutexLock(carbon_mutex_t *mux);
void CarbonMutexUnlock(carbon_mutex_t *mux);

// Related to condition variables
void CarbonCondInit(carbon_cond_t *cond);
void CarbonCondWait(carbon_cond_t *cond, carbon_mutex_t *mux);
void CarbonCondSignal(carbon_cond_t *cond);
void CarbonCondBroadcast(carbon_cond_t *cond);

// Related to barriers
void CarbonBarrierInit(carbon_barrier_t *barrier, unsigned int count);
void CarbonBarrierWait(carbon_barrier_t *barrier);

// Related to Mutexes
void __CarbonMutexInit(UInt64 time, core_id_t core_id, carbon_mutex_t *mux);
void __CarbonMutexLock(UInt64 time, core_id_t core_id, carbon_mutex_t *mux);
void __CarbonMutexUnlock(UInt64 time, core_id_t core_id, carbon_mutex_t *mux);

// Related to condition variables
void __CarbonCondInit(UInt64 time, core_id_t core_id, carbon_cond_t *cond);
void __CarbonCondWait(UInt64 time, core_id_t core_id, carbon_cond_t *cond, carbon_mutex_t *mux);
void __CarbonCondSignal(UInt64 time, core_id_t core_id, carbon_cond_t *cond);
void __CarbonCondBroadcast(UInt64 time, core_id_t core_id, carbon_cond_t *cond);

// Related to barriers
void __CarbonBarrierInit(UInt64 time, core_id_t core_id, carbon_barrier_t *barrier, unsigned int count);
void __CarbonBarrierWait(UInt64 time, core_id_t core_id, carbon_barrier_t *barrier);

#ifdef __cplusplus
}
#endif

#endif
