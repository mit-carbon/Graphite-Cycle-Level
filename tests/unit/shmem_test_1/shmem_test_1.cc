#include "instruction.h"
#include "performance_model.h"
#include "simulator.h"
#include "core_manager.h"
#include "core.h"
#include "packetize.h"
#include "app_request.h"
#include "thread_interface.h"
#include "carbon_user.h"
#include "fixed_types.h"

Instruction* _instruction = NULL;
UInt32 _variable = 0;

SInt32 _numInstructions = 1000;
SInt32 _numThreads = 64;

void* threadMain(void*);

int main(int argc, char* argv[])
{
   CarbonStartSim(argc, argv);
   Simulator::enablePerformanceModels();

   OperandList operandList;
   operandList.push_back(Operand(Operand::MEMORY, 0, Operand::READ));
   operandList.push_back(Operand(Operand::MEMORY, 0, Operand::WRITE));
   
   _instruction = new GenericInstruction(operandList);

   core_id_t threads[_numThreads-1];
   for (SInt32 i = 0; i < _numThreads-1; i++)
   {
      fprintf(stderr, "Spawning Thread\n");
      threads[i] = CarbonSpawnThread(threadMain, NULL);
      fprintf(stderr, "Spawned Thread(%i)\n", threads[i]);
   }
   threadMain(NULL);

   for (SInt32 i = 0; i < _numThreads-1; i++)
   {
      fprintf(stderr, "Joining Thread(%i)\n", threads[i]);
      CarbonJoinThread(threads[i]);
      fprintf(stderr, "Joined Thread(%i)\n", threads[i]);
   }

   Simulator::disablePerformanceModels();
   CarbonStopSim();

   return 0;
}

void* threadMain(void*)
{
   Core* core = Sim()->getCoreManager()->getCurrentCore();

   for (SInt32 i = 0; i < _numInstructions; i++)
   {
      UnstructuredBuffer* instruction_info = new UnstructuredBuffer();
      
      PerformanceModel::MemoryAccessList* memoryAccessList = new PerformanceModel::MemoryAccessList(); 
      IntPtr address = (IntPtr) &_variable;
      UInt32 size = (UInt32) sizeof(_variable);
      memoryAccessList->push_back(make_pair(address, size));
      memoryAccessList->push_back(make_pair(address, size));
      
      (*instruction_info) << _instruction << true << memoryAccessList;
     
      LOG_PRINT("Sending Instruction for Modeling"); 
      AppRequest app_request(AppRequest::HANDLE_INSTRUCTION, instruction_info);
      Sim()->getThreadInterface(core->getId())->sendAppRequest(app_request);
   }

   return (void*) NULL;
}
