#pragma once

#include <pin.H>
#include "instruction.h"
#include "fixed_types.h"

void handleInstruction(Instruction* instruction, bool atomic_memory_update, UInt32 num_memory_args, ...);
void handleBranch(bool taken, IntPtr target);
void fillOperandListMemOps(OperandList* list, INS ins);
UInt32 fillMemInfo(IARGLIST& iarg_memory_info, INS ins);
void fillOperandList(OperandList* list, INS ins);
void addInstructionModeling(INS ins);
