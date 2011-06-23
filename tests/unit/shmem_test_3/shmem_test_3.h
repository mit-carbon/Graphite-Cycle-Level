#pragma once

#include <stdlib.h>
#include <vector>
using std::vector;

#include "event.h"
#include "fixed_types.h"
#include "core.h"
#include "rand_num.h"

#define EVENT_START_SIMULATION               100
#define EVENT_ISSUE_MEMORY_ACCESS            101

void debug_printf(const char* fmt, ...);

void readConfigurationParameters(int argc, char* argv[]);
void printHelpMessage();
void waitForCompletion();

void processStartSimulationEvent(Event* event);
void processIssueMemoryAccessEvent(Event* event);
void processCompleteMemoryAccessEvent(Event* event);

class SyntheticCore
{
public:
   enum MemoryOperation
   {
      MEMORY_READ = 0,
      MEMORY_WRITE,
      NUM_MEMORY_OPERATIONS
   };
   
   SyntheticCore(Core* core,
                 vector<IntPtr>& rd_only_shared_address_list,
                 vector<IntPtr>& rd_wr_shared_address_list,
                 vector<IntPtr>& private_memory_address_list);
   ~SyntheticCore();

   void startSimulation();
   void processIssueMemoryAccessEvent(UInt64 time, IntPtr address, MemoryOperation memory_operation);
   void processCompleteMemoryAccessEvent(UInt64 time, IntPtr address, MemoryOperation memory_operation);

private:
   class MemoryRequest
   {
   public:
      MemoryRequest()
         : _time(0), _address(INVALID_ADDRESS), _memory_operation(NUM_MEMORY_OPERATIONS) {}
      MemoryRequest(UInt64 time, IntPtr address, MemoryOperation memory_operation)
         : _time(time), _address(address), _memory_operation(memory_operation) {}
      ~MemoryRequest() {}

      UInt64 _time;
      IntPtr _address;
      MemoryOperation _memory_operation;
   };

   Core* _core;
   UInt64 _core_time;

   bool isSimulationComplete();
   void endSimulation();

   void initiateMemoryAccess(UInt64 time, IntPtr address, MemoryOperation memory_operation);
};
