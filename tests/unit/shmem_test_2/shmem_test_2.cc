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

Instruction* _readInstruction = NULL;
Instruction* _writeInstruction = NULL;
UInt32 _val = 0;

carbon_mutex_t _mutex;

SInt32 _numIterations = 1;
SInt32 _numThreads = 1;

void* threadMain(void*);

int main(int argc, char* argv[])
{
   CarbonStartSim(argc, argv);
   Simulator::enablePerformanceModels();

   OperandList readOperandList;
   readOperandList.push_back(Operand(Operand::MEMORY, 0, Operand::READ));
   _readInstruction = new GenericInstruction(readOperandList);
   
   OperandList writeOperandList;
   writeOperandList.push_back(Operand(Operand::MEMORY, 0, Operand::WRITE));
   _writeInstruction = new GenericInstruction(writeOperandList);

   CarbonMutexInit(&_mutex);

   core_id_t threads[_numThreads-1];
   for (SInt32 i = 0; i < _numThreads-1; i++)
   {
      fprintf(stderr, "Spawning Thread\n");
      threads[i] = CarbonSpawnThread(threadMain, NULL);
      fprintf(stderr, "Spawned Thread(%i)\n", threads[i]);
      // sleep(1);
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

   fprintf(stderr, "Val(%u)\n", _val);

   return 0;
}

void* threadMain(void*)
{
   Core* core = Sim()->getCoreManager()->getCurrentCore();

   for (SInt32 i = 0; i < _numIterations; i++)
   {
      // Lock Mutex
      CarbonMutexLock(&_mutex);

      // (Address,Size)
      IntPtr address = (IntPtr) &_val;
      UInt32 size = (UInt32) sizeof(_val);
      
      // Read Instruction
      UnstructuredBuffer* readInstructionInfo = new UnstructuredBuffer();
      
      PerformanceModel::MemoryAccessList* readMemoryAccessList = new PerformanceModel::MemoryAccessList(); 
      readMemoryAccessList->push_back(make_pair(address, size));
      
      (*readInstructionInfo) << _readInstruction << false << readMemoryAccessList;
     
      AppRequest readAppRequest(AppRequest::HANDLE_INSTRUCTION, readInstructionInfo);
      Sim()->getThreadInterface(core->getId())->sendAppRequest(readAppRequest);

      fprintf(stderr, "_val(%u)\n", _val);
      // Add 1 to val
      _val += 1;
      fprintf(stderr, "_val(%u)\n", _val);

      // Write Instruction
      UnstructuredBuffer* writeInstructionInfo = new UnstructuredBuffer();
      
      PerformanceModel::MemoryAccessList* writeMemoryAccessList = new PerformanceModel::MemoryAccessList(); 
      writeMemoryAccessList->push_back(make_pair(address, size));
      
      (*writeInstructionInfo) << _writeInstruction << false << writeMemoryAccessList;
     
      AppRequest writeAppRequest(AppRequest::HANDLE_INSTRUCTION, writeInstructionInfo);
      Sim()->getThreadInterface(core->getId())->sendAppRequest(writeAppRequest);

      // Unlock Mutex
      CarbonMutexUnlock(&_mutex);
      fprintf(stderr, "_val(%u)\n", _val);
   }

   return (void*) NULL;
}
