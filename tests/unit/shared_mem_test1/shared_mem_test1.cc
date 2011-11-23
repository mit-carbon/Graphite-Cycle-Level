#include "core.h"
#include "mem_component.h"
#include "core_manager.h"
#include "event_manager.h"
#include "simulator.h"
#include "event.h"
#include "carbon_user.h"
#include "fixed_types.h"

using namespace std;

IntPtr address = 0x1000;

UInt32 write_val_0;
UInt32 read_val_0;
UInt32 write_val_1;
UInt32 read_val_1;

typedef struct
{
   core_id_t core_id;
   Core::mem_op_t mem_op_type;
   IntPtr address;
   Byte* buf;
   UInt32 size;
   UInt32 value;
} MemoryRequest;

void initiateMemoryAccess(UInt64 time, UInt32 access_id);
void completeMemoryAccess(Event* event);
void waitForCompletion();

MemoryRequest memory_requests[] = 
{
   { 0, Core::WRITE, address, (Byte*) &write_val_0, sizeof(write_val_0), 100 },
   { 0, Core::READ,  address, (Byte*) &read_val_0,  sizeof(read_val_0),  100 },
   { 1, Core::READ,  address, (Byte*) &read_val_1,  sizeof(read_val_1),  100 },
   { 1, Core::WRITE, address, (Byte*) &write_val_1, sizeof(write_val_1), 110 },
   { 0, Core::READ,  address, (Byte*) &read_val_0,  sizeof(read_val_0),  110 }
};


void initiateMemoryAccess(UInt64 time, UInt32 access_id)
{
   if (access_id == 5)
      return;

   Core* core = Sim()->getCoreManager()->getCoreFromID(memory_requests[access_id].core_id);
   Core::mem_op_t mem_op_type = memory_requests[access_id].mem_op_type;
   IntPtr address = memory_requests[access_id].address;
   Byte* buf = memory_requests[access_id].buf;
   UInt32 size = memory_requests[access_id].size;
   UInt32 value = memory_requests[access_id].value;
   if (mem_op_type == Core::WRITE)
      *((UInt32*) buf) = value;

   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) << core
                 << access_id
                 << MemComponent::L1_DCACHE << Core::NONE << mem_op_type
                 << address << buf << size
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

   Core::mem_op_t mem_op_type = memory_requests[access_id].mem_op_type;
   Byte* buf = memory_requests[access_id].buf;
   UInt32 value = memory_requests[access_id].value;

   if (mem_op_type == Core::READ)
   {
      LOG_ASSERT_ERROR(*((UInt32*) buf) == value, "Expected(%u) != Actual(%u)",
                       value, *((UInt32*) buf));
   }

   access_id ++;
   initiateMemoryAccess(time, access_id);
}

void waitForCompletion()
{
   // Sleep in a loop for 100 milli-sec each. Wake up and see if done
   while (1)
   {
      usleep(1000);
      if (! Sim()->getEventManager()->hasEventsPending())
         break;
   }
}

int main (int argc, char *argv[])
{
   printf("Starting (shared_mem_test1)\n");

   // Start the simulator
   CarbonStartSim(argc, argv);

   // Enable Performance Models
   Simulator::__enablePerformanceModels();

   // Register Event Handlers
   Event::registerHandler(Event::COMPLETE_MEMORY_ACCESS, completeMemoryAccess);

   initiateMemoryAccess(0, 0);

   // Wait till no more events are present
   waitForCompletion();

   // Unregister Event Handlers
   Event::unregisterHandler(Event::COMPLETE_MEMORY_ACCESS);

   // Disable Performance Models
   Simulator::__disablePerformanceModels();
   
   // Stop the simulator
   CarbonStopSim();
   
   printf("Finished (shared_mem_test1) - SUCCESS\n");
   return 0;
}
   
void accessMemory(UInt64 time, Core* core, Core::mem_op_t mem_op_type, IntPtr address, Byte* buf, UInt32 size)
{
}
