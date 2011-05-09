#ifndef DVFS_H
#define DVFS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "fixed_types.h"

void CarbonGetCoreFrequency(float* frequency);
void CarbonSetCoreFrequency(float* frequency);

void __CarbonGetCoreFrequency(core_id_t core_id, float* frequency);
void __CarbonSetCoreFrequency(core_id_t core_id, float* frequency);

#ifdef __cplusplus
}
#endif

#endif // DVFS_H
