#include <syscall.h>
using namespace std;

#include "handle_syscalls.h"
#include "simulator.h"
#include "core_manager.h"
#include "core.h"
#include "syscall_client.h"
#include "syscall_manager.h"
#include "log.h"

void handleSyscall(CONTEXT* ctxt)
{
   IntPtr syscall_number = PIN_GetContextReg(ctxt, REG_GAX);
   LOG_PRINT("handleSyscall(%lli)", syscall_number);
   
   Core* core = Sim()->getCoreManager()->getCurrentCore();
   assert(core);
   
   // Save the syscall number
   core->getSyscallClient()->saveSyscallNumber(syscall_number);

   // Only Handled Syscall Right now is SYS_futex
   if (syscall_number != SYS_futex)
      return;
   
   SyscallManager::syscall_args_t args;

#ifdef TARGET_IA32
   args.arg0 = PIN_GetContextReg (ctxt, REG_GBX);
   args.arg1 = PIN_GetContextReg (ctxt, REG_GCX);
   args.arg2 = PIN_GetContextReg (ctxt, REG_GDX);
   args.arg3 = PIN_GetContextReg (ctxt, REG_GSI);
   args.arg4 = PIN_GetContextReg (ctxt, REG_GDI);
   args.arg5 = PIN_GetContextReg (ctxt, REG_GBP);
#endif

#ifdef TARGET_X86_64
   // FIXME: The LEVEL_BASE:: ugliness is required by the fact that REG_R8 etc 
   // are also defined in /usr/include/sys/ucontext.h
   args.arg0 = PIN_GetContextReg (ctxt, LEVEL_BASE::REG_GDI);
   args.arg1 = PIN_GetContextReg (ctxt, LEVEL_BASE::REG_GSI);
   args.arg2 = PIN_GetContextReg (ctxt, LEVEL_BASE::REG_GDX);
   args.arg3 = PIN_GetContextReg (ctxt, LEVEL_BASE::REG_R10); 
   args.arg4 = PIN_GetContextReg (ctxt, LEVEL_BASE::REG_R8);
   args.arg5 = PIN_GetContextReg (ctxt, LEVEL_BASE::REG_R9);
#endif

   UnstructuredBuffer* syscall_info = new UnstructuredBuffer();      
   (*syscall_info) << syscall_number << args;

   LOG_PRINT("Syscall(%lli), Args(%llx,%llx,%llx,%llx,%llx,%llx)", \
         syscall_number, args.arg0, args.arg1, args.arg2, args.arg3, args.arg4, args.arg5);

   // Send Request to Handle Syscall to Sim Thread 
   AppRequest app_request(AppRequest::HANDLE_SYSCALL, syscall_info);
   Sim()->getThreadInterface(core->getId())->sendAppRequest(app_request);

   // Recv reply from the Sim Thread
   SimReply sim_reply = Sim()->getThreadInterface(core->getId())->recvSimReply();

   // Analyze Reply
   int syscall_retval = (int) sim_reply;

   // Save syscall number and ret val in the syscall_client
   core->getSyscallClient()->saveSyscallRetval(syscall_retval);
}

void syscallEnterRunModel(THREADID threadIndex, CONTEXT* ctxt, SYSCALL_STANDARD syscall_standard, void*)
{
   IntPtr syscall_number = PIN_GetSyscallNumber(ctxt, syscall_standard);
   LOG_PRINT("syscallEnterRunModel(%lli)", syscall_number);
   if (syscall_number == SYS_futex)
   {
      PIN_SetSyscallNumber(ctxt, syscall_standard, SYS_getpid);
   }
}

void syscallExitRunModel(THREADID threadIndex, CONTEXT* ctxt, SYSCALL_STANDARD syscall_standard, void*)
{
   Core* core = Sim()->getCoreManager()->getCurrentCore();
   assert(core);

   IntPtr syscall_number = core->getSyscallClient()->retrieveSyscallNumber();
   LOG_PRINT("syscallExitRunModel(%lli)", syscall_number);
   if (syscall_number == SYS_futex)
   {
      IntPtr syscall_retval = core->getSyscallClient()->retrieveSyscallRetval(); 
      PIN_SetContextReg(ctxt, REG_GAX, syscall_retval);

      LOG_PRINT("Syscall(%lli) returned (%lli)", syscall_number, syscall_retval);
   }
}
