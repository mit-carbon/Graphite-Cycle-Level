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
void computeAddressLists(vector<vector<IntPtr> >& rd_only_shared_address_list,
                         vector<vector<IntPtr> >& rd_wr_shared_address_list,
                         vector<vector<IntPtr> >& private_address_list);
void waitForCompletion();

void processStartSimulationEvent(Event* event);
void processIssueMemoryAccessEvent(Event* event);
void processCompleteMemoryAccessEvent(Event* event);

class SyntheticCore
{
public:
   enum InstructionType
   {
      NON_MEMORY,
      RD_ONLY_SHARED_MEMORY_READ,
      RD_WR_SHARED_MEMORY_READ,
      RD_WR_SHARED_MEMORY_WRITE,
      PRIVATE_MEMORY_READ,
      PRIVATE_MEMORY_WRITE,
      NUM_INSTRUCTION_TYPES
   };

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

   vector<IntPtr> _rd_only_shared_address_list;
   vector<IntPtr> _rd_wr_shared_address_list;
   vector<IntPtr> _private_address_list;

   SInt32 _num_outstanding_loads;
   SInt32 _num_outstanding_stores;

   bool _is_memory_request_blocked;
   MemoryRequest _blocked_memory_request;

   SInt32 _total_instructions_issued;
   SInt32 _total_instructions_executed;
   vector<SInt32> _num_instructions_executed_list;
   UInt64 _total_memory_request_blockage_time;

   RandNum _instruction_generator_rand_num;
   RandNum _rd_only_shared_address_generator_rand_num;
   RandNum _rd_wr_shared_address_generator_rand_num;
   RandNum _private_address_generator_rand_num;

   InstructionType getRandomInstructionType();
   IntPtr getRandomReadOnlySharedAddress();
   IntPtr getRandomReadWriteSharedAddress();
   IntPtr getRandomPrivateAddress();

   bool isSimulationComplete();
   void endSimulation();

   void generateInstructions(UInt64 time);
   void initiateMemoryAccess(UInt64 time, IntPtr address, MemoryOperation memory_operation);
   void createMemoryAccessInstruction(InstructionType instruction_type,
                                      IntPtr& address, MemoryOperation& memory_operation);

   void waitForPreviousLoads(UInt64 time, IntPtr address);
   void waitForPreviousStores(UInt64 time, IntPtr address);
};
