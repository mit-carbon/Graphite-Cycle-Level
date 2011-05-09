#include "simulator.h"
#include "core.h"
#include "performance_model.h"
#include "routine_manager.h"
#include "syscall_manager.h"
#include "thread_manager.h"
#include "app_request.h"

bool
AppRequest::process(Core* req_core)
{
   assert (_type >= 0 && _type < NUM_TYPES);

   bool cont = false;

   LOG_PRINT("App Request: Type(%u), CoreID(%i)", _type, req_core->getId());

   switch (_type)
   {
   case HANDLE_INSTRUCTION:
      {
         Instruction* ins;
         bool atomic_memory_update;
         PerformanceModel::MemoryAccessList* memory_access_list;
         (*_request) >> ins >> atomic_memory_update >> memory_access_list;

         // FIXME: Set 'cont' here
         req_core->getPerformanceModel()->handleInstruction(ins, atomic_memory_update, memory_access_list);
         break;
      }

   case EMULATE_ROUTINE:
      {
         Routine::Id routine_id;
         (*_request) >> routine_id;
         UnstructuredBuffer* routine_args = _request;

         UInt64 time = req_core->getPerformanceModel()->getTime();

         __emulateRoutine(time, req_core->getId(), routine_id, routine_args);
         break;
      }

   case HANDLE_SYSCALL:
      {
         IntPtr syscall_number;
         SyscallManager::syscall_args_t args;
         (*_request) >> syscall_number >> args;

         UInt64 time = req_core->getPerformanceModel()->getTime();

         Sim()->getSyscallManager()->handleSyscall(time, req_core->getId(), syscall_number, args);
         break;
      }

   case PROCESS_THREAD_EXIT:
      {
         Sim()->getThreadManager()->onThreadExit(req_core->getId());
         break;
      }

   default:
      {
         LOG_PRINT_ERROR("Unrecognized App Request(%u)", _type);
         break;
      }
   }

   // Delete _request
   if (_request)
      delete _request;

   return cont;
}
