#pragma once

#include <pin.H>
#include "instruction.h"
#include "fixed_types.h"

namespace CycleAccurate
{

void handleInstruction(Instruction* instruction, bool is_atomic_update, UInt32 num_memory_args, ...);
void handleBranch(bool taken, IntPtr target);
void fillOperandListMemOps(OperandList* list, INS ins);
void fillMemInfo(IARGLIST& iarg_memory_info, INS ins);
void fillOperandList(OperandList* list, INS ins);
void addInstructionModeling(INS ins);

}
