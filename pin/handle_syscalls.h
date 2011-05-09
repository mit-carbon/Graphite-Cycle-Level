#pragma once

#include "pin.H"
#include "fixed_types.h"

void handleSyscall(CONTEXT* ctxt);
void syscallEnterRunModel(THREADID threadIndex, CONTEXT* ctxt, SYSCALL_STANDARD syscall_standard, void* v);
void syscallExitRunModel(THREADID threadIndex, CONTEXT* ctxt, SYSCALL_STANDARD syscall_standard, void* v);
