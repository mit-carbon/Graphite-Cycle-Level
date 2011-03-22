#include "core.h"
#include "mem_component.h"
#include "core_manager.h"
#include "simulator.h"

#include "carbon_user.h"
#include "fixed_types.h"

using namespace std;

void* thread_func(void*);

int num_threads = 2;
int num_iterations = 1;

carbon_barrier_t barrier;

IntPtr address = 0x1000;

int main (int argc, char *argv[])
{
   CarbonStartSim(argc, argv);
   printf("Starting (shared_mem_test4)\n");

   CarbonBarrierInit(&barrier, num_threads);

   carbon_thread_t tid_list[num_threads];

   Core* core = Sim()->getCoreManager()->getCurrentCore();

   int val = 0;
   core->initiateMemoryAccess(0, MemComponent::L1_DCACHE, Core::NONE, Core::WRITE, address, (Byte*) &val, sizeof(val));
   LOG_PRINT("Core(%i): Access Time(%llu)", core->getId(), core->getShmemPerfModel()->getCycleCount());

   for (int i = 0; i < num_threads; i++)
   {
      tid_list[i] = CarbonSpawnThread(thread_func, (void*) i);
   }

   for (int i = 0; i < num_threads; i++)
   {
      CarbonJoinThread(tid_list[i]);
   }
  
   core->initiateMemoryAccess(0, MemComponent::L1_DCACHE, Core::NONE, Core::READ, address, (Byte*) &val, sizeof(val));
   LOG_PRINT("Core(%i): Access Time(%llu)", core->getId(), core->getShmemPerfModel()->getCycleCount());
   
   printf("val = %i\n", val);
   if (val != (num_iterations))
   {
      printf("shared_mem_test4 (FAILURE)\n");
   }
   else
   {
      printf("shared_mem_test4 (SUCCESS)\n");
   }
  
   CarbonStopSim();
   return 0;
}

void* thread_func(void* threadid)
{
   long tid = (long) threadid;
   Core* core = Sim()->getCoreManager()->getCurrentCore();

   for (int i = 0; i < num_iterations; i++)
   {
      int val;
      core->initiateMemoryAccess(0, MemComponent::L1_DCACHE, Core::NONE, Core::READ, address, (Byte*) &val, sizeof(val));
      LOG_PRINT("Core(%i): Access Time(%llu)", core->getId(), core->getShmemPerfModel()->getCycleCount());

      CarbonBarrierWait(&barrier); 

      if (tid == 0)
      {
         val ++;
         core->initiateMemoryAccess(0, MemComponent::L1_DCACHE, Core::NONE, Core::WRITE, address, (Byte*) &val, sizeof(val));
         LOG_PRINT("Core(%i): Access Time(%llu)", core->getId(), core->getShmemPerfModel()->getCycleCount());
      }
   }
   return NULL;
}
