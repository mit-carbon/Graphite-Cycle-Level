#include "core.h"
#include "mem_component.h"
#include "core_manager.h"
#include "simulator.h"

#include "carbon_user.h"
#include "fixed_types.h"

using namespace std;

void* thread_func(void*);

IntPtr address = 0x1000;
int num_threads = 100;
int num_iterations = 100;

int main (int argc, char *argv[])
{
   CarbonStartSim(argc, argv);
   printf("Starting (shared_mem_test2)\n");

   carbon_thread_t tid_list[num_threads];

   Core* core = Sim()->getCoreManager()->getCurrentCore();
   int val = 0;
   core->initiateMemoryAccess(0, MemComponent::L1_DCACHE, Core::NONE, Core::WRITE, address, (Byte*) &val, sizeof(val));

   for (int i = 0; i < num_threads; i++)
   {
      tid_list[i] = CarbonSpawnThread(thread_func, (void*) i);
   }

   for (int i = 0; i < num_threads; i++)
   {
      CarbonJoinThread(tid_list[i]);
   }
   
   core->initiateMemoryAccess(0, MemComponent::L1_DCACHE, Core::NONE, Core::READ, address, (Byte*) &val, sizeof(val));
   
   printf("val(%i)\n", val);
   if (val == (num_threads * num_iterations))
   {
      printf("shared_mem_test2 (SUCCESS)\n");
   }
   else
   {
      printf("shared_mem_test2 (FAILURE)\n");
      assert(false);
   }
   
   CarbonStopSim();
   return 0;
}

void* thread_func(void*)
{
   Core* core = Sim()->getCoreManager()->getCurrentCore();

   for (int i = 0; i < num_iterations; i++)
   {
      int val;
      core->initiateMemoryAccess(0, MemComponent::L1_DCACHE, Core::LOCK, Core::READ_EX, address, (Byte*) &val, sizeof(val));
      
      val += 1;

      core->initiateMemoryAccess(0, MemComponent::L1_DCACHE, Core::UNLOCK, Core::WRITE, address, (Byte*) &val, sizeof(val));
   }
}
