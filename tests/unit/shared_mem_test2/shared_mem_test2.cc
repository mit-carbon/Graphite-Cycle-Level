#include "core.h"
#include "mem_component.h"
#include "core_manager.h"
#include "simulator.h"
#include "event.h"
#include "event_manager.h"
#include "utils.h"
#include "fixed_types.h"
#include "carbon_user.h"

using namespace std;

void initializeMemory();
void startSimulation(UInt64 time);
void checkResult();
void __checkResult();
void initiateMemoryAccess(Core* core, UInt64 time, UInt32 access_id);
void completeMemoryAccess(Event* event);
void waitForCompletion();
void printHelpMessage();

IntPtr _address = 0x1000;
UInt32 _num_cores = 100;
UInt32 _num_iterations = 100;
vector<UInt32> _buf_list;
UInt64 _max_time = 0;

int main (int argc, char *argv[])
{
   printf("Starting (shared_mem_test2)\n");

   // Start the simulator
   CarbonStartSim(argc, argv);

   // Enable the performance models
   Simulator::__enablePerformanceModels();
 
   // Read Command Line Arguments
   for (SInt32 i = 1; i < argc-1; i += 2)
   {
      if (string(argv[i]) == "-i")
         _num_iterations = atoi(argv[i+1]);
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

   // Get the number of cores from the config object
   _num_cores = Config::getSingleton()->getTotalCores();

   // Register Event Handlers
   Event::registerHandler(Event::COMPLETE_MEMORY_ACCESS, completeMemoryAccess);

   _buf_list.resize(_num_cores, 0);

   // Initialize memory and start simulation after memory is initialized
   initializeMemory();
   waitForCompletion();
   
   // Check the final result
   checkResult();
   waitForCompletion();
  
   // Unregister Event Handlers
   Event::unregisterHandler(Event::COMPLETE_MEMORY_ACCESS);

   // Disable Performance Models
   Simulator::__disablePerformanceModels();

   // Stop the simulator 
   CarbonStopSim();

   return 0;
}

void printHelpMessage()
{
   fprintf(stderr, "[Usage]: ./shared_mem_test2 -i <number of iterations>\n");
}

void initializeMemory()
{
   _buf_list[0] = 0;
   Byte* buf = (Byte*) &_buf_list[0];
   UInt32 size = sizeof(_buf_list[0]);
   Core* core = Sim()->getCoreManager()->getCoreFromID(0);
   assert(core);

   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) << core
                 << 0
                 << MemComponent::L1_DCACHE << Core::NONE << Core::WRITE
                 << _address << buf << size
                 << true;
   EventInitiateMemoryAccess* event = new EventInitiateMemoryAccess(0, event_args);
   Event::processInOrder(event, 0, EventQueue::ORDERED);
}

void startSimulation(UInt64 time)
{
   LOG_PRINT("startSimulation(%llu)", time);
   for (UInt32 i = 1; i < _num_cores; i++)
   {
      Core* core = Sim()->getCoreManager()->getCoreFromID(i);
      assert(core);
      initiateMemoryAccess(core, time, 0);
   }
}

void checkResult()
{
   Byte* buf = (Byte*) &_buf_list[0];
   UInt32 size = sizeof(_buf_list[0]);
   Core* core = Sim()->getCoreManager()->getCoreFromID(0);
   assert(core);

   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) << core
                 << 1
                 << MemComponent::L1_DCACHE << Core::NONE << Core::READ
                 << _address << buf << size
                 << true;
   EventInitiateMemoryAccess* event = new EventInitiateMemoryAccess(_max_time, event_args);
   Event::processInOrder(event, 0, EventQueue::ORDERED);
}

void __checkResult()
{
   printf("Final Answer(%u)\n", _buf_list[0]);
   if (_buf_list[0] == ((_num_cores-1) * _num_iterations))
   {
      printf("shared_mem_test2 (SUCCESS)\n");
   }
   else
   {
      printf("shared_mem_test2 (FAILURE)\n");
      LOG_PRINT_ERROR("Abort");
   }
}

void initiateMemoryAccess(Core* core, UInt64 time, UInt32 access_id)
{
   LOG_PRINT("initiateMemoryAccess[Core Id(%i), Time(%llu), Access Id(%u)]", core->getId(), time, access_id);
   if (access_id == (2 * _num_iterations))
      return;

   Core::mem_op_t mem_op_type;
   Core::lock_signal_t lock_signal;
   if (access_id % 2 == 0)
   {
      mem_op_type = Core::READ_EX;
      lock_signal = Core::LOCK;
   }
   else
   {
      mem_op_type = Core::WRITE;
      lock_signal = Core::UNLOCK;
   }

   Byte* buf = (Byte*) &_buf_list[core->getId()];
   UInt32 size = sizeof(_buf_list[core->getId()]);

   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) << core
                 << access_id
                 << MemComponent::L1_DCACHE << lock_signal << mem_op_type
                 << _address << buf << size
                 << true;
   EventInitiateMemoryAccess* event = new EventInitiateMemoryAccess(time, event_args);
   Event::processInOrder(event, core->getId(), EventQueue::ORDERED);
}

void completeMemoryAccess(Event* event)
{
   Core* core;
   UInt32 access_id;
   UInt64 time = event->getTime();
   UnstructuredBuffer* event_args = event->getArgs();
   (*event_args) >> core >> access_id;
   assert(core);

   LOG_PRINT("Complete Memory Access[Core(%i), Time(%llu), Access Id(%u)]", core->getId(), time, access_id);

   _max_time = getMax<UInt64>(_max_time, time);

   if (core->getId() == 0)
   {
      if (access_id == 0)
         startSimulation(time);
      else if (access_id == 1)
         __checkResult();
      else
         LOG_PRINT_ERROR("Unrecognized Access Id(%u)", access_id);
   }
   else
   {
      if (access_id % 2 == 0)
      {
         // This is a READ_EX. Increment variable by 1
         _buf_list[core->getId()] ++;
      }
      access_id ++;
      initiateMemoryAccess(core, time, access_id);
   }
}

void waitForCompletion()
{
   // Sleep in a loop for 1 milli-sec each. Wake up and see if done
   while (1)
   {
      usleep(1000);
      if (! Sim()->getEventManager()->hasEventsPending())
         break;
   }
}
