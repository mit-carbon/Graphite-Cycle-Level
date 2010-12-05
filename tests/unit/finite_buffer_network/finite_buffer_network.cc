#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include "simulator.h"
#include "core_manager.h"
#include "config.h"
#include "core.h"
#include "network.h"
#include "network_model.h"
#include "router.h"
#include "router_performance_model.h"
#include "flow_control_scheme.h"
#include "clock_skew_minimization_object.h"
#include "finite_buffer_network_model.h"
#include "carbon_user.h"
#include "utils.h"

#define MAX_DATA_LENGTH 100

enum NetworkTrafficType
{
   UNIFORM_RANDOM = 0,
   BIT_COMPLEMENT,
   SHUFFLE,
   TRANSPOSE,
   TORNADO,
   NEAREST_NEIGHBOR,
   NUM_NETWORK_TRAFFIC_TYPES
};

void* sendNetworkTraffic(void*);
void uniformRandomTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);
void bitComplementTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);
void shuffleTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);
void transposeTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);
void tornadoTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);
void nearestNeighborTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);

SInt32 getRandNum(SInt32 start, SInt32 end, struct drand48_data* rand_num_state);
void synchronize(UInt64 time, Core* core);

UInt64 _inter_packet_delay;
carbon_barrier_t _global_barrier;
PacketType _packet_type = USER_2;
int _num_cores;

int main(int argc, char* argv[])
{
   CarbonStartSim(argc, argv);

   Simulator::enablePerformanceModelsInCurrentProcess();

   _inter_packet_delay = (UInt64) atoi(argv[1]);
   
   _num_cores = (int) Config::getSingleton()->getApplicationCores();
   CarbonBarrierInit(&_global_barrier, _num_cores);

   carbon_thread_t tid_list[_num_cores-1];
   for (SInt32 i = 0; i < _num_cores-1; i++)
   {
      tid_list[i] = CarbonSpawnThread(sendNetworkTraffic, NULL);
   }
   sendNetworkTraffic(NULL);

   for (SInt32 i = 0; i < _num_cores-1; i++)
   {
      CarbonJoinThread(tid_list[i]);
   }
   
   printf("Joined all threads\n");

   Simulator::disablePerformanceModelsInCurrentProcess();

   CarbonStopSim();
 
   return 0;
}

void* sendNetworkTraffic(void*)
{
   // Wait for everyone to be spawned
   CarbonBarrierWait(&_global_barrier);

   Core* core = Sim()->getCoreManager()->getCurrentCore();
   
   int network_traffic_type = UNIFORM_RANDOM;
   vector<int> send_vec;
   vector<int> receive_vec;
   // Generate the Network Traffic
   switch (network_traffic_type)
   {
      case UNIFORM_RANDOM:
         uniformRandomTrafficGenerator(core->getId(), send_vec, receive_vec);
         break;
      case BIT_COMPLEMENT:
         bitComplementTrafficGenerator(core->getId(), send_vec, receive_vec);
         break;
      case SHUFFLE:
         shuffleTrafficGenerator(core->getId(), send_vec, receive_vec);
         break;
      case TRANSPOSE:
         transposeTrafficGenerator(core->getId(), send_vec, receive_vec);
         break;
      case TORNADO:
         tornadoTrafficGenerator(core->getId(), send_vec, receive_vec);
         break;
      case NEAREST_NEIGHBOR:
         nearestNeighborTrafficGenerator(core->getId(), send_vec, receive_vec);
         break;
      default:
         assert(false);
         break;
   }

   // Starting timestamp
   UInt64 packet_injection_time = _inter_packet_delay;

   Byte data[MAX_DATA_LENGTH + 1];
   SInt32 num_packets = 5; // Total sent packets per core
   SInt32 outstanding_window_size = 5; // Number of outstanding packets
   for (SInt32 i = 0; i < num_packets + outstanding_window_size; i++)
   {
      if (i < num_packets)
      {
         SInt32 data_length = 8; // 1 flit
         // OLD: getRandNum(1, MAX_DATA_LENGTH, &rand_num_state);
         SInt32 receiver = send_vec[i % send_vec.size()]; 
        
         NetPacket net_packet(packet_injection_time, _packet_type, core->getId(), receiver, data_length, data);
         core->getNetwork()->netSend(net_packet);
    
         synchronize(packet_injection_time, core);
      
         packet_injection_time += _inter_packet_delay;
      }

      if (i >= outstanding_window_size)
      {
         SInt32 sender = receive_vec[(i-outstanding_window_size) % receive_vec.size()];
         NetPacket recv_net_packet = core->getNetwork()->netRecv(sender, _packet_type);
         delete [] (Byte*) recv_net_packet.data;
      }
   }

   // Wait for everyone to finish
   CarbonBarrierWait(&_global_barrier);
   return NULL;
}

// Generate integers from [start,end]
SInt32 getRandNum(SInt32 start, SInt32 end, struct drand48_data* rand_num_state)
{
   double result;
   drand48_r(rand_num_state, &result);
   return (start + (SInt32) (result * (end - start + 1)));
}
 
void synchronize(UInt64 packet_injection_time, Core* core)
{
   ClockSkewMinimizationClient* clock_skew_client = core->getClockSkewMinimizationClient();

   if (clock_skew_client)
      clock_skew_client->synchronize(packet_injection_time);
}

void computeEMeshTopologyParams(int num_cores, int& mesh_width, int& mesh_height);
void computeEMeshPosition(int core_id, int& sx, int& sy, int mesh_width);
int computeCoreId(int sx, int sy, int mesh_width);

void uniformRandomTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec)
{
   // Generate Random Numbers using Linear Congruential Generator
   int send_matrix[_num_cores][_num_cores];
   int receive_matrix[_num_cores][_num_cores];

   send_matrix[0][0] = _num_cores / 2; // Initial seed
   receive_matrix[0][send_matrix[0][0]] = 0;
   for (int i = 0; i < _num_cores; i++) // Time Slot
   {
      if (i != 0)
      {
         send_matrix[i][0] = send_matrix[i-1][1];
         receive_matrix[i][send_matrix[i][0]] = 0;
      }
      for (int j = 1; j < _num_cores; j++) // Sender
      {
         send_matrix[i][j] = (13 * send_matrix[i][j-1] + 5) % _num_cores;
         receive_matrix[i][send_matrix[i][j]] = j;
      }
   }

   // Check the validity of the random numbers
   for (int i = 0; i < _num_cores; i++) // Time Slot
   {
      vector<bool> bits(_num_cores, false);
      for (int j = 0; j < _num_cores; j++) // Sender
      {
         bits[send_matrix[i][j]] = true;
      }
      for (int j = 0; j < _num_cores; j++)
      {
         assert(bits[j]);
      }
   }

   for (int j = 0; j < _num_cores; j++) // Sender
   {
      vector<bool> bits(_num_cores, false);
      for (int i = 0; i < _num_cores; i++) // Time Slot
      {
         bits[send_matrix[i][j]] = true;
      }
      for (int i = 0; i < _num_cores; i++)
      {
         assert(bits[i]);
      }
   }

   for (SInt32 i = 0; i < _num_cores; i++)
   {
      send_vec.push_back(send_matrix[i][core_id]);
      receive_vec.push_back(receive_matrix[i][core_id]);
   }
}

void bitComplementTrafficGenerator(int core_id, vector<core_id_t>& send_vec, vector<core_id_t>& receive_vec)
{
   assert(isPower2(_num_cores));
   int mask = _num_cores-1;
   int dst_core = (~core_id) & mask;
   send_vec.push_back(dst_core);
   receive_vec.push_back(dst_core);
}

void shuffleTrafficGenerator(int core_id, vector<core_id_t>& send_vec, vector<core_id_t>& receive_vec)
{
   assert(isPower2(_num_cores));
   int mask = _num_cores-1;
   int nbits = floorLog2(_num_cores);
   int dst_core = ((core_id >> (nbits-1)) & 1) | ((core_id << 1) & mask);
   send_vec.push_back(dst_core); 
   receive_vec.push_back(dst_core); 
}

void transposeTrafficGenerator(int core_id, vector<core_id_t>& send_vec, vector<core_id_t>& receive_vec)
{
   int mesh_width, mesh_height;
   computeEMeshTopologyParams(_num_cores, mesh_width, mesh_height);
   int sx, sy;
   computeEMeshPosition(core_id, sx, sy, mesh_width);
   int dst_core = computeCoreId(sy, sx, mesh_width);
   
   send_vec.push_back(dst_core);
   receive_vec.push_back(dst_core);
}

void tornadoTrafficGenerator(int core_id, vector<core_id_t>& send_vec, vector<core_id_t>& receive_vec)
{
   int mesh_width, mesh_height;
   computeEMeshTopologyParams(_num_cores, mesh_width, mesh_height);
   int sx, sy;
   computeEMeshPosition(core_id, sx, sy, mesh_width);
   int dst_core = computeCoreId((sx + mesh_width/2) % mesh_width, (sy + mesh_height/2) % mesh_height, mesh_width);
}

void nearestNeighborTrafficGenerator(int core_id, vector<core_id_t>& send_vec, vector<core_id_t>& receive_vec)
{
   int mesh_width, mesh_height;
   computeEMeshTopologyParams(_num_cores, mesh_width, mesh_height);
   int sx, sy;
   computeEMeshPosition(core_id, sx, sy, mesh_width);
   int dst_core = computeCoreId((sx+1) % mesh_width, (sy+1) % mesh_height, mesh_width);

   send_vec.push_back(dst_core);
   receive_vec.push_back(dst_core);
}

void computeEMeshTopologyParams(int num_cores, int& mesh_width, int& mesh_height)
{
   mesh_width = (int) sqrt((float) num_cores);
   mesh_height = (int) ceil(1.0 * num_cores / mesh_width);
   assert(num_cores == (mesh_width * mesh_height));
}

void computeEMeshPosition(int core_id, int& sx, int& sy, int mesh_width)
{
   sx = core_id % mesh_width;
   sy = core_id / mesh_width;
}

int computeCoreId(int sx, int sy, int mesh_width)
{
   return ((sy * mesh_width) + sx);
}
