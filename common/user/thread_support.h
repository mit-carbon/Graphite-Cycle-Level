#ifndef THREAD_SUPPORT_H
#define THREAD_SUPPORT_H

#include "fixed_types.h"

typedef SInt32 carbon_thread_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*thread_func_t)(void *);

typedef struct
{
   core_id_t core_id;
   thread_func_t func;
   void* arg;
} ThreadSpawnRequest;

core_id_t CarbonSpawnThread(thread_func_t func, void *arg);
void CarbonJoinThread(core_id_t tid);

void __CarbonSpawnThread(UInt64 time, core_id_t req_core_id, thread_func_t func, void* arg);
void __CarbonJoinThread(UInt64 time, core_id_t req_core_id, core_id_t join_core_id);

#ifdef __cplusplus
}
#endif

#endif
