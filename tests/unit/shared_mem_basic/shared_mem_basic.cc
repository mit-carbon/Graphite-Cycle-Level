#include "core.h"
#include "mem_component.h"
#include "core_manager.h"
#include "simulator.h"
#include "config.h"

#include "carbon_user.h"
#include "fixed_types.h"

using namespace std;

int main (int argc, char *argv[])
{
   printf("Starting (shared_mem_basic)\n");
   CarbonStartSim(argc, argv);

   UInt32 address = 0x1000;

   // 1) Get a core object
   Core* core = Sim()->getCoreManager()->getCoreFromID(0);
   for (UInt32 i = 0; i < Config::getSingleton()->getTotalCores(); i++)
   {
      Sim()->getCoreManager()->getCoreFromID(i)->enablePerformanceModels();
   }

   UInt32 written_val = 100;
   UInt32 read_val = 0;

   printf("Writing(%u) into address(0x%x)\n", written_val, address);
   // Write some value into this address
   core->initiateMemoryAccess(0, MemComponent::L1_DCACHE, Core::NONE, Core::WRITE, address, (Byte*) &written_val, sizeof(written_val));
   printf("Writing(%u) into address(0x%x) completed\n", written_val, address);

   // Read out the value
   printf("Reading from address(0x%x)\n", address);
   core->initiateMemoryAccess(0, MemComponent::L1_DCACHE, Core::NONE, Core::READ, address, (Byte*) &read_val, sizeof(read_val));
   printf("Reading(%u) from address(0x%x) completed\n", read_val, address);
   assert(read_val == 100);

   printf("Finished (shared_mem_basic) - SUCCESS\n");
   return 0;
}
