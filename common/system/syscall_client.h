#pragma once

#include <iostream>
#include "fixed_types.h"

class SyscallClient
{
public:
   SyscallClient(): _syscall_number(0), _syscall_retval(0) {}
   ~SyscallClient() {}

   void saveSyscallNumber(IntPtr syscall_number)
   { _syscall_number = syscall_number; }
   IntPtr retrieveSyscallNumber()
   { return _syscall_number; }

   void saveSyscallRetval(IntPtr syscall_retval)
   { _syscall_retval = syscall_retval; }
   IntPtr retrieveSyscallRetval()
   { return _syscall_retval; }

private:
   IntPtr _syscall_number;
   IntPtr _syscall_retval;
};
