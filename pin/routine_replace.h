#pragma once

#include <pthread.h>

#include "pin.H"
#include "carbon_user.h"
#include "fixed_types.h"

void routineCallback(RTN rtn, void* v);

core_id_t emulateCarbonSpawnThread(CONTEXT* context, thread_func_t thread_func, void* arg);
int emulatePthreadCreate(CONTEXT* context, pthread_t* thread_ptr, pthread_attr_t* attr, thread_func_t thread_func, void* arg);
void emulateCarbonJoinThread(CONTEXT* context, core_id_t join_core_id);
int emulatePthreadJoin(CONTEXT* context, pthread_t thread, void** thead_return);
IntPtr nullFunction();

AFUNPTR getFunptr(CONTEXT* context, string func_name);
