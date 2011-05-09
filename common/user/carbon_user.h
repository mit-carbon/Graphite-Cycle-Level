#ifndef CARBON_USER_H
#define CARBON_USER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <pthread.h>
#include "fixed_types.h"
#include "capi.h"
#include "sync_api.h"
#include "thread_support.h"
#include "perf_counter_support.h"
#include "dvfs.h"

SInt32 CarbonStartSim(int argc, char **argv);
void CarbonStopSim();
UInt64 CarbonGetTime();

UInt64 __CarbonGetTime(core_id_t core_id);

#ifdef __cplusplus
}
#endif

#endif // CARBON_USER_H
