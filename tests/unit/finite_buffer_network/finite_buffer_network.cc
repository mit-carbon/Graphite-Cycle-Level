#include <stdio.h>
#include <stdlib.h>
#include "simulator.h"
#include "core_manager.h"
#include "config.h"
#include "core.h"
#include "network.h"
#include "network_model.h"
#include "router.h"
#include "router_performance_model.h"
#include "flow_control_scheme.h"
#include "buffer_model.h"
#include "clock_skew_minimization_object.h"
#include "finite_buffer_network_model.h"
#include "carbon_user.h"

#define MAX_DATA_LENGTH 100

void* randomNetTraffic(void*);
SInt32 getRandNum(SInt32 start, SInt32 end, struct drand48_data* rand_num_state);

UInt64 _inter_packet_delay;
UInt64 _inter_barrier_delay;
PacketType _packet_type = USER_2; 

int main(int argc, char* argv[])
{
   CarbonStartSim(argc, argv);

   Simulator::enablePerformanceModelsInCurrentProcess();

   _inter_packet_delay = (UInt64) atoi(argv[1]);
   _inter_barrier_delay = (UInt64) Sim()->getCfg()->getInt("clock_skew_minimization/barrier/quantum");

   SInt32 num_cores = (SInt32) Config::getSingleton()->getApplicationCores();
  
   printf("num_cores(%i)\n", num_cores);

   carbon_thread_t tid_list[num_cores-1];
   for (SInt32 i = 0; i < num_cores-1; i++)
   {
      tid_list[i] = CarbonSpawnThread(randomNetTraffic, NULL);
   }
   randomNetTraffic(NULL);

   for (SInt32 i = 0; i < num_cores-1; i++)
   {
      CarbonJoinThread(tid_list[i]);
   }
   
   printf("Joined all threads\n");

   Simulator::disablePerformanceModelsInCurrentProcess();

   printf("Before CarbonStopSim()\n");
   CarbonStopSim();
   printf("After CarbonStopSim()\n");
   
   return 0;
}

void* randomNetTraffic(void*)
{
   // Wait for everyone to be spawned
   sleep(5);

   Core* core = Sim()->getCoreManager()->getCurrentCore();
   printf("Core(%i)\n", core->getId());
   ClockSkewMinimizationClient* clock_skew_client = core->getClockSkewMinimizationClient();
   
   // Random Number Generator State
   struct drand48_data rand_num_state;
   srand48_r(core->getId() + 1, &rand_num_state);
   
   // Starting timestamp
   UInt64 time = _inter_packet_delay;

   Byte data[MAX_DATA_LENGTH + 1];
   SInt32 num_packets = 100; // 100;
   for (SInt32 i = 0; i < num_packets; i++)
   {
      SInt32 data_length = 10; // getRandNum(1, MAX_DATA_LENGTH, &rand_num_state);
      SInt32 receiver = getRandNum(0, Config::getSingleton()->getApplicationCores() - 1, &rand_num_state);
     
      UInt64 packet_injection_time = time; 
      NetPacket net_packet(time, _packet_type, core->getId(), receiver, data_length, data);
      printf("Sending packet from %i -> %i(%u), time(%llu), length(%i, %i)\n", \
          core->getId(), receiver, Config::getSingleton()->getApplicationCores(), \
          time, \
          data_length, core->getNetwork()->getModeledLength(net_packet));
      core->getNetwork()->netSend(net_packet);

      for ( ; time < (packet_injection_time + _inter_packet_delay); time += _inter_barrier_delay)
      {
         // First Router, First Channel
         FiniteBufferNetworkModel* network_model = (FiniteBufferNetworkModel*) \
                                                   core->getNetwork()->getNetworkModelFromPacketType(_packet_type);
         while (1)
         {
            UInt64 queue_time = network_model->getRouter(0)->getRouterPerformanceModel()->getFlowControlObject()-> \
                                getBufferModel(0)->getBufferTime();
            bool queue_empty = network_model->getRouter(0)->getRouterPerformanceModel()->getFlowControlObject()-> \
                               getBufferModel(0)->empty();
            
            if ((queue_empty) || (time <= queue_time))
            {
               // Wait till the queue time becomes equal to the core time
               if (clock_skew_client)
                  clock_skew_client->synchronize(time);
               break;
            }
            else
            {
               // printf("time(%llu), queue_time(%llu), empty(%i)\n", time, queue_time, queue_empty);
               sched_yield();
            }
         }
      }
   }
   printf("Core(%i) OVER\n", core->getId());
   sleep(5);
   return NULL;
}

// Generate integers from [start,end]
SInt32 getRandNum(SInt32 start, SInt32 end, struct drand48_data* rand_num_state)
{
   double result;
   drand48_r(rand_num_state, &result);
   return (start + (SInt32) (result * (end - start + 1)));
}
