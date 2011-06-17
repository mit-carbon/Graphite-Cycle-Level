#include <stdarg.h>
#include <iostream>
#include <fstream>
#include "carbon_user.h"
#include "simulator.h"
#include "core_manager.h"
#include "config.h"
#include "utils.h"
#include "dynamic_instruction_info.h"
#include "lock.h"
#include "semaphore.h"
#include "synthetic_memory.h"

// #define DEBUG 1

// 1/3rd instructions are memory accesses

// 2/3rd of all memory accesses are loads
// 1/3rd of all memory accesses are stores

// 1/2 of memory accesses are to private data
// 1/2 of memory accesses are to shared data

// Configuration Parameters
SInt32 _num_cores = 1;
SInt32 _degree_of_sharing = 1;
SInt32 _num_shared_addresses = 1;
SInt32 _num_private_addresses = 1;
SInt32 _total_instructions_per_core = 1;
SInt32 _max_outstanding_loads = 1;
SInt32 _max_outstanding_stores = 1;
SInt32 _cache_block_size;

float _fraction_read_only_shared_addresses;
float _instruction_type_probabilities[SyntheticCore::NUM_INSTRUCTION_TYPES];

// Synchronization
Lock _lock;
Semaphore _semaphore;

// Synthetic Core List
vector<SyntheticCore*> _synthetic_core_list;

// Completion Time
UInt64 _completion_time = 0;

int main(int argc, char* argv[])
{
   // Initialize the Simulator
   CarbonStartSim(argc, argv);

   // Read Configuration Parameters
   readConfigurationParameters(argc, argv);

   // Compute Address Lists that each core accesses
   vector<vector<IntPtr> > rd_only_shared_address_list;
   vector<vector<IntPtr> > rd_wr_shared_address_list;
   vector<vector<IntPtr> > private_address_list;
   computeAddressLists(rd_only_shared_address_list, rd_wr_shared_address_list, private_address_list);

   // Instantiate Synthetic Core objects
   for (SInt32 i = 0; i < _num_cores; i++)
   {
      SyntheticCore* synthetic_core = 
         new SyntheticCore(Sim()->getCoreManager()->getCoreFromID(i),
                           rd_only_shared_address_list[i],
                           rd_wr_shared_address_list[i],
                           private_address_list[i]);
      _synthetic_core_list.push_back(synthetic_core);
   }

   // Enable all the simulation models
   Simulator::__enablePerformanceModels();

   // Register Event Handlers
   Event::registerHandler(EVENT_START_SIMULATION, processStartSimulationEvent);
   Event::registerHandler(EVENT_ISSUE_MEMORY_ACCESS, processIssueMemoryAccessEvent);
   Event::registerHandler(Event::COMPLETE_MEMORY_ACCESS, processCompleteMemoryAccessEvent);

   // Create First Event
   Event* event = new Event((Event::Type) EVENT_START_SIMULATION, 0 /* time */);
   Event::processInOrder(event, 0 /* core_id */, EventQueue::ORDERED);

   // Wait for all cores to finish processing events
   waitForCompletion();
  
   // Unregister Event Handler
   Event::unregisterHandler(Event::COMPLETE_MEMORY_ACCESS);
   Event::unregisterHandler(EVENT_ISSUE_MEMORY_ACCESS);
   Event::unregisterHandler(EVENT_START_SIMULATION);

   printf("Success: Completion Time (%llu)\n", (long long unsigned int) _completion_time);

   // Disable all the simulation models
   Simulator::__disablePerformanceModels();
   debug_printf("Disabled Performance Models\n");

   CarbonStopSim();
   debug_printf("CarbonStopSim() over\n");

   return 0;
}

void debug_printf(const char* fmt, ...)
{
#ifdef DEBUG
   va_list ap;
   va_start(ap, fmt);

   vfprintf(stderr, fmt, ap);
   va_end(ap);
#endif
}

void waitForCompletion()
{
   debug_printf("Wait Starting\n");
   for (SInt32 i = 0; i < _num_cores; i++)
      _semaphore.wait();
   debug_printf("Wait Completed [Num Cores(%i)]\n", _num_cores);
}

void readConfigurationParameters(int argc, char* argv[])
{
   // -t : num threads
   // -d : degree of sharing
   // -ns : number of shared addresses
   // -np : number of private addresses
   // -N : total number of instructions per core
   // -nol : maximum number of outstanding loads
   // -nos : maximum number of outstanding stores
  
   // Read Command Line Arguments
   for (SInt32 i = 1; i < argc-1; i += 2)
   {
      if (string(argv[i]) == "-t")
         _num_cores = atoi(argv[i+1]);
      else if (string(argv[i]) == "-d")
         _degree_of_sharing = atoi(argv[i+1]);
      else if (string(argv[i]) == "-ns")
         _num_shared_addresses = atoi(argv[i+1]);
      else if (string(argv[i]) == "-np")
         _num_private_addresses = atoi(argv[i+1]);
      else if (string(argv[i]) == "-N")
         _total_instructions_per_core = atoi(argv[i+1]);
      else if (string(argv[i]) == "-nol")
         _max_outstanding_loads = atoi(argv[i+1]);
      else if (string(argv[i+1]) == "-nos")
         _max_outstanding_stores = atoi(argv[i+1]);
      else if (string(argv[i]) == "-c") // Simulator arguments
         break;
      else if (string(argv[i]) == "-h")
      {
         printHelpMessage();
         exit(0);
      }
      else
      {
         fprintf(stderr, "** ERROR **\n");
         printHelpMessage();
         exit(-1);
      }
   }

   LOG_ASSERT_ERROR(_num_cores >= _degree_of_sharing, "num_cores(%i), degree_of_sharing(%i)",
         _num_cores, _degree_of_sharing);

   LOG_ASSERT_ERROR(isPower2(_num_shared_addresses) && isPower2(_num_private_addresses),
         "Should be power of 2. num_shared_addresses(%i), _num_private_addresses(%i)",
         _num_shared_addresses, _num_private_addresses);

   ifstream instruction_probabilites_file;
   instruction_probabilites_file.open("./tests/benchmarks/synthetic_memory/instruction_probabilities.dat");
   // This is initially a discrete probablity function
   for (UInt32 i = 0; i < SyntheticCore::NUM_INSTRUCTION_TYPES; i++)
      instruction_probabilites_file >> _instruction_type_probabilities[i];
   instruction_probabilites_file.close();
   
   // Do this calculation before converting them into cumulative probabilites
   _fraction_read_only_shared_addresses =
      _instruction_type_probabilities[SyntheticCore::RD_ONLY_SHARED_MEMORY_READ] /
      (_instruction_type_probabilities[SyntheticCore::RD_ONLY_SHARED_MEMORY_READ] +
       _instruction_type_probabilities[SyntheticCore::RD_WR_SHARED_MEMORY_READ] +
       _instruction_type_probabilities[SyntheticCore::RD_WR_SHARED_MEMORY_WRITE]);
   
   // Convert instruction type probabilities into cumulative probabilities
   for (UInt32 i = 1; i < SyntheticCore::NUM_INSTRUCTION_TYPES; i++)
      _instruction_type_probabilities[i] += _instruction_type_probabilities[i-1];
   
   printf("Instruction Probabilities:\n");
   for (SInt32 i = 0; i < SyntheticCore::NUM_INSTRUCTION_TYPES; i++)
      printf("Probability[%i] -> %f\n", i, _instruction_type_probabilities[i]);
   printf("\n");
   
   // FIXME: Check this !
   _cache_block_size = Sim()->getCfg()->getInt("perf_model/l1_dcache/T1/cache_block_size", 0);
   assert(_cache_block_size != 0);
   
   printf("Num Cores(%i)\nDegree of Sharing(%i)\nNum Shared Addresses(%i)\nNum Private Addresses(%i)\nTotal Instructions per Core(%i)\nCache Block Size(%u)\n", \
         _num_cores, _degree_of_sharing, \
         _num_shared_addresses, _num_private_addresses, \
         _total_instructions_per_core, _cache_block_size);
}

void printHelpMessage()
{
   fprintf(stderr, "[Usage]: ./synthetic_memory -t <arg1> -d <arg2> -ns <arg3> -np <arg4> -N <arg5> -nol <arg6> -nos <arg7>\n");
   fprintf(stderr, "where <arg1> = Number of Cores (default 1)\n");
   fprintf(stderr, " and  <arg2> = Degree of Application Sharing (default 1)\n");
   fprintf(stderr, " and  <arg3> = Number of Shared Memory Addresses (default 1)\n");
   fprintf(stderr, " and  <arg4> = Number of Private Memory Addresses (default 1)\n");
   fprintf(stderr, " and  <arg5> = Number of Instructions Executed per core (default 1)\n");
   fprintf(stderr, " and  <arg6> = Number of Outstanding Loads (default 1)\n");
   fprintf(stderr, " and  <arg7> = Number of Outstanding Stores (default 1)\n");
}

void computeAddressLists(vector<vector<IntPtr> >& rd_only_shared_address_list,
                         vector<vector<IntPtr> >& rd_wr_shared_address_list,
                         vector<vector<IntPtr> >& private_address_list)
{
   rd_only_shared_address_list.resize(_num_cores);
   rd_wr_shared_address_list.resize(_num_cores);
   private_address_list.resize(_num_cores);

   RandNum core_id_generator_rand_num(0, _num_cores);

   // This is pre-computed before the application starts
   for (SInt32 i = 0; i < _num_shared_addresses; i++)
   {
      IntPtr shared_address = i * _cache_block_size;
      for (SInt32 j = 0; j < _degree_of_sharing; j++)
      {
         core_id_t core_id = (core_id_t) core_id_generator_rand_num.next();
         
         if (i < ((SInt32) (_fraction_read_only_shared_addresses * _num_shared_addresses)))
         {
            rd_only_shared_address_list[core_id].push_back(shared_address);
         }
         else
         {
            rd_wr_shared_address_list[core_id].push_back(shared_address);
         }
      }
   }
   
   for (SInt32 i = 0; i < _num_cores; i++)
   {
      for (SInt32 j = 0; j < _num_private_addresses; j++)
      {
         SInt32 private_address_index = i * _num_private_addresses + j;
         IntPtr private_address = ((IntPtr) (_num_shared_addresses + private_address_index)) * _cache_block_size;
         private_address_list[i].push_back(private_address);
      }
   }
}

void processStartSimulationEvent(Event* event)
{
   assert(event->getType() == EVENT_START_SIMULATION);
   assert(event->getTime() == 0);

   for (SInt32 i = 0; i < _num_cores; i++)
   {
      _synthetic_core_list[i]->startSimulation();
   }
}

void processIssueMemoryAccessEvent(Event* event)
{
   assert(event->getType() == EVENT_ISSUE_MEMORY_ACCESS);

   UInt64 time = event->getTime();
   UnstructuredBuffer* event_args = event->getArgs();

   Core* core;
   IntPtr address;
   SyntheticCore::MemoryOperation memory_operation;

   (*event_args) >> core >> address >> memory_operation;

   _synthetic_core_list[core->getId()]->processIssueMemoryAccessEvent(time, address, memory_operation); 
}

void processCompleteMemoryAccessEvent(Event* event)
{
   assert(event->getType() == Event::COMPLETE_MEMORY_ACCESS);
   
   UInt64 time = event->getTime();
   UnstructuredBuffer* event_args = event->getArgs();

   DynamicInstructionInfo info;
   Core* core;

   (*event_args) >> core >> info;

   assert((info.type == DynamicInstructionInfo::MEMORY_READ) || (info.type == DynamicInstructionInfo::MEMORY_WRITE));
   SyntheticCore::MemoryOperation memory_operation = (info.type == DynamicInstructionInfo::MEMORY_READ) ?
                                                     SyntheticCore::MEMORY_READ : SyntheticCore::MEMORY_WRITE;
   
   _synthetic_core_list[core->getId()]->processCompleteMemoryAccessEvent(time, info.memory_info.addr, memory_operation);
}

// SyntheticCore
SyntheticCore::SyntheticCore(Core* core,
                             vector<IntPtr>& rd_only_shared_address_list,
                             vector<IntPtr>& rd_wr_shared_address_list,
                             vector<IntPtr>& private_address_list)
   : _core(core)
   , _core_time(0)
   , _last_memory_access_id(0)
   , _rd_only_shared_address_list(rd_only_shared_address_list)
   , _rd_wr_shared_address_list(rd_wr_shared_address_list)
   , _private_address_list(private_address_list)
   , _num_outstanding_loads(0)
   , _num_outstanding_stores(0)
   , _is_memory_request_blocked(false)
   , _total_instructions_issued(0)
   , _total_instructions_executed(0)
   , _total_memory_request_blockage_time(0)
{
   _instruction_generator_rand_num = RandNum(0, 1, _core->getId());
   _rd_only_shared_address_generator_rand_num = RandNum(0, _rd_only_shared_address_list.size(), _core->getId());
   _rd_wr_shared_address_generator_rand_num = RandNum(0, _rd_wr_shared_address_list.size(), _core->getId());
   _private_address_generator_rand_num = RandNum(0, _private_address_list.size(), _core->getId());

   _num_instructions_executed_list.resize(NUM_INSTRUCTION_TYPES);
   for (UInt32 i = 0; i < NUM_INSTRUCTION_TYPES; i++)
      _num_instructions_executed_list[i] = 0;
}

void
SyntheticCore::startSimulation()
{
   generateInstructions(0);
   if (isSimulationComplete())
      endSimulation();
}

bool
SyntheticCore::isSimulationComplete()
{
   return ( (_num_outstanding_loads == 0) && 
            (_num_outstanding_stores == 0) && 
            (_total_instructions_executed == _total_instructions_per_core) );
}

void
SyntheticCore::endSimulation()
{
   _lock.acquire();
   _completion_time = getMax<UInt64>(_completion_time, _core_time);
   _lock.release();

   debug_printf("Signaling on Semaphore: Core(%i)\n", _core->getId());
   // Signal all events completed
   _semaphore.signal();
}

void
SyntheticCore::processIssueMemoryAccessEvent(UInt64 time, IntPtr address, MemoryOperation memory_operation)
{
   switch (memory_operation)
   {
   case MEMORY_READ:
      
      if (_num_outstanding_loads < _max_outstanding_loads)
      {
         initiateMemoryAccess(time, address, memory_operation);
         generateInstructions(time + 1);
      }
      else if (_num_outstanding_loads == _max_outstanding_loads)
      {
         waitForPreviousLoads(time, address);
      }
      else
      {
         LOG_PRINT_ERROR("Reached Illegal State, Num Outstanding Loads(%i), Max Outstanding Loads(%i)",
               _num_outstanding_loads, _max_outstanding_loads);
      }
      
      break;

   case MEMORY_WRITE:
      
      if (_num_outstanding_stores < _max_outstanding_stores)
      {
         initiateMemoryAccess(time, address, memory_operation);
         generateInstructions(time + 1);
      }
      else if (_num_outstanding_stores == _max_outstanding_stores)
      {
         waitForPreviousStores(time, address);
      }
      else
      {
         LOG_PRINT_ERROR("Reached Illegal State, Num Outstanding Stores(%i), Max Outstanding Stores(%i)",
               _num_outstanding_stores, _max_outstanding_stores);
      }

      break;

   default:
      LOG_PRINT_ERROR("Unrecognized Memory Operation (%u)", memory_operation);
   }

   assert(!isSimulationComplete());
}

void
SyntheticCore::processCompleteMemoryAccessEvent(UInt64 time, IntPtr address, MemoryOperation memory_operation)
{
   switch (memory_operation)
   {
   case MEMORY_READ:
      _num_outstanding_loads --;
      if (_is_memory_request_blocked && (_blocked_memory_request._memory_operation == MEMORY_READ))
      {
         assert(time >= _blocked_memory_request._time);
         _total_memory_request_blockage_time += (time - _blocked_memory_request._time);

         initiateMemoryAccess(time, _blocked_memory_request._address, _blocked_memory_request._memory_operation);
         _is_memory_request_blocked = false;
         
         generateInstructions(time + 1);

         assert(!isSimulationComplete());
      }
      break;

   case MEMORY_WRITE:
      _num_outstanding_stores --;
      if (_is_memory_request_blocked && (_blocked_memory_request._memory_operation == MEMORY_WRITE))
      {
         assert(time >= _blocked_memory_request._time);
         _total_memory_request_blockage_time += (time - _blocked_memory_request._time);
         
         initiateMemoryAccess(time, _blocked_memory_request._address, _blocked_memory_request._memory_operation);
         _is_memory_request_blocked = false;
         
         generateInstructions(time + 1);

         assert(!isSimulationComplete());
      }
      break;

   default:
      LOG_PRINT_ERROR("memory_operation should be MEMORY_READ (or) MEMORY_WRITE");
   }

   // Complete MEMORY instruction
   _total_instructions_executed ++;

   // Update the Core Time
   _core_time = getMax<UInt64>(_core_time, time + 1);

   if (isSimulationComplete())
      endSimulation();
}

void
SyntheticCore::initiateMemoryAccess(UInt64 time, IntPtr address, MemoryOperation memory_operation)
{
   Core::mem_op_t core_mem_op;
   if (memory_operation == MEMORY_READ)
   {
      core_mem_op = Core::READ;
      _num_outstanding_loads ++;
   }
   else // (memory_operation == MEMORY_WRITE)
   {
      core_mem_op = Core::WRITE;
      _num_outstanding_stores ++;
   }
   
   SInt32 buffer = 0;

   // Initiate Memory Access Event
   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) << _core
                 << _last_memory_access_id ++
                 << MemComponent::L1_DCACHE << Core::NONE << core_mem_op
                 << address << &buffer << sizeof(buffer)
                 << true;
   EventInitiateMemoryAccess* event = new EventInitiateMemoryAccess(time, event_args);
   Event::processInOrder(event, _core->getId(), EventQueue::ORDERED);
}

void
SyntheticCore::generateInstructions(UInt64 time)
{
   while (_total_instructions_issued < _total_instructions_per_core)
   {
      InstructionType instruction_type = getRandomInstructionType();
      _num_instructions_executed_list[instruction_type] ++;
      _total_instructions_issued ++;

      debug_printf("Created Instruction [Core Id(%i), Type(%u)]\n", _core->getId(), instruction_type);
     
      if (instruction_type == NON_MEMORY)
      {
         // Complete Non-Memory Instruction
         _total_instructions_executed ++;
         time ++;
      } 
      else // (instruction_type != NON_MEMORY)
      {
         IntPtr address;
         MemoryOperation memory_operation;
         createMemoryAccessInstruction(instruction_type, address, memory_operation);

         debug_printf("Created Memory Access Instruction [Core Id(%i), Time(%llu), Address(0x%llx), MemoryOperation(%u)]\n", \
               _core->getId(), (long long unsigned int) time, address, memory_operation);
         
         UnstructuredBuffer* event_args = new UnstructuredBuffer();
         (*event_args) << _core << address << memory_operation;
         Event* event = new Event((Event::Type) EVENT_ISSUE_MEMORY_ACCESS, time, event_args);
         Event::processInOrder(event, _core->getId(), EventQueue::ORDERED);
         
         break;
      }
   }

   // Update the Core Time
   _core_time = getMax<UInt64>(_core_time, time);
}

void
SyntheticCore::createMemoryAccessInstruction(InstructionType instruction_type,
                                             IntPtr& address, MemoryOperation& memory_operation)
{
   switch (instruction_type)
   {
   case RD_ONLY_SHARED_MEMORY_READ:
      
      assert(_rd_only_shared_address_list.size() != 0);
      address = getRandomReadOnlySharedAddress();
      memory_operation = MEMORY_READ;
      break;

   case RD_WR_SHARED_MEMORY_READ:
      
      assert(_rd_wr_shared_address_list.size() != 0);
      address = getRandomReadWriteSharedAddress();
      memory_operation = MEMORY_READ;
      break;
      
   case RD_WR_SHARED_MEMORY_WRITE:
         
      assert(_rd_wr_shared_address_list.size() != 0);
      address = getRandomReadWriteSharedAddress();
      memory_operation = MEMORY_WRITE;
      break;

   case PRIVATE_MEMORY_READ:
         
      address = getRandomPrivateAddress();
      memory_operation = MEMORY_READ;
      break;
   
   case PRIVATE_MEMORY_WRITE:
         
      address = getRandomPrivateAddress();
      memory_operation = MEMORY_WRITE;
      break;

   default:
      LOG_PRINT_ERROR("Unrecognized Instruction Type(%u)", instruction_type);
   }
}

void
SyntheticCore::waitForPreviousLoads(UInt64 time, IntPtr address)
{
   assert(!_is_memory_request_blocked);
   _is_memory_request_blocked = true;
   _blocked_memory_request = MemoryRequest(time, address, MEMORY_READ);
}

void
SyntheticCore::waitForPreviousStores(UInt64 time, IntPtr address)
{
   assert(!_is_memory_request_blocked);
   _is_memory_request_blocked = true;
   _blocked_memory_request = MemoryRequest(time, address, MEMORY_WRITE);
}

SyntheticCore::InstructionType
SyntheticCore::getRandomInstructionType()
{  
   double rand_num = _instruction_generator_rand_num.next();
   for (SInt32 i = 0; i < NUM_INSTRUCTION_TYPES; i++)
   {
      if (rand_num < _instruction_type_probabilities[i])
         return (InstructionType) i;
   }
   assert(false);
   return NUM_INSTRUCTION_TYPES;
}

IntPtr
SyntheticCore::getRandomReadOnlySharedAddress()
{
   UInt32 index = (UInt32) _rd_only_shared_address_generator_rand_num.next();
   assert(index < _rd_only_shared_address_list.size());
   return _rd_only_shared_address_list[index];
}

IntPtr
SyntheticCore::getRandomReadWriteSharedAddress()
{
   UInt32 index = (UInt32) _rd_wr_shared_address_generator_rand_num.next();
   assert(index < _rd_wr_shared_address_list.size());
   return _rd_wr_shared_address_list[index];
}

IntPtr
SyntheticCore::getRandomPrivateAddress()
{
   UInt32 index = (UInt32) _private_address_generator_rand_num.next();
   assert(index < _private_address_list.size());
   return _private_address_list[index];
   
}
