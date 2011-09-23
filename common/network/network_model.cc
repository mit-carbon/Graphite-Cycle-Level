#include <cassert>
#include <cmath>
using namespace std;

#include "simulator.h"
#include "core_manager.h"
#include "network_types.h"
#include "network_model_magic.h"
#include "network_model_emesh_hop_counter.h"
#include "finite_buffer_network_model_emesh.h"
#include "finite_buffer_network_model_clos.h"
#include "finite_buffer_network_model_atac.h"
#include "finite_buffer_network_model_flip_atac.h"
#include "network.h"
#include "memory_manager.h"
#include "clock_converter.h"
#include "utils.h"
#include "log.h"

NetworkModel::NetworkModel(Network *network, SInt32 network_id, bool is_finite_buffer)
   : _enabled(false)
   , _network(network)
   , _network_id(network_id)
   , _is_finite_buffer(is_finite_buffer)
{
   _core_id = _network->getCore()->getId();
   
   if (network_id == 0)
      _network_name = "USER_1";
   else if (network_id == 1)
      _network_name = "USER_2";
   else if (network_id == 2)
      _network_name = "MEMORY_1";
   else if (network_id == 3)
      _network_name = "MEMORY_2";
   else if (network_id == 4)
      _network_name = "SYSTEM";
   else
      LOG_PRINT_ERROR("Unrecognized Network Num(%u)", network_id);

   _tile_width = Sim()->getCfg()->getFloat("general/tile_width");

   // Performance Counters
   initializePerformanceCounters();
}

NetworkModel*
NetworkModel::createModel(Network *net, SInt32 network_id, UInt32 model_type)
{
   switch (model_type)
   {
   case NETWORK_MAGIC:
      return new NetworkModelMagic(net, network_id);

   case NETWORK_EMESH_HOP_COUNTER:
      return new NetworkModelEMeshHopCounter(net, network_id);

   case FINITE_BUFFER_NETWORK_EMESH:
      return new FiniteBufferNetworkModelEMesh(net, network_id);

   case FINITE_BUFFER_NETWORK_ATAC:
      return new FiniteBufferNetworkModelAtac(net, network_id);

   case FINITE_BUFFER_NETWORK_CLOS:
	  return new FiniteBufferNetworkModelClos(net, network_id);
     
   case FINITE_BUFFER_NETWORK_FLIP_ATAC:
      return new FiniteBufferNetworkModelFlipAtac(net, network_id);
   
   default:
      LOG_PRINT_ERROR("Unrecognized Network Model(%u)", model_type);
      return NULL;
   }
}

UInt32 
NetworkModel::parseNetworkType(string str)
{
   if (str == "magic")
      return NETWORK_MAGIC;
   else if (str == "emesh_hop_counter")
      return NETWORK_EMESH_HOP_COUNTER;
   else if (str == "finite_buffer_emesh")
      return FINITE_BUFFER_NETWORK_EMESH;
   else if (str == "finite_buffer_atac")
      return FINITE_BUFFER_NETWORK_ATAC;
   else if(str == "finite_buffer_clos")
	  return FINITE_BUFFER_NETWORK_CLOS;
   else if (str == "finite_buffer_flip_atac")
     return FINITE_BUFFER_NETWORK_FLIP_ATAC;
   else
   {
      fprintf(stderr, "Unrecognized Network Type(%s)\n", str.c_str());
      exit(EXIT_FAILURE);
   }
}

pair<bool,SInt32>
NetworkModel::computeCoreCountConstraints(UInt32 network_type, SInt32 core_count)
{
   switch (network_type)
   {
      case NETWORK_MAGIC:
      case NETWORK_EMESH_HOP_COUNTER:
         return make_pair(false,core_count);

      case FINITE_BUFFER_NETWORK_EMESH:
         return FiniteBufferNetworkModelEMesh::computeCoreCountConstraints(core_count);

      case FINITE_BUFFER_NETWORK_ATAC:
         return FiniteBufferNetworkModelAtac::computeCoreCountConstraints(core_count);

      case FINITE_BUFFER_NETWORK_CLOS:
         return FiniteBufferNetworkModelClos::computeCoreCountConstraints(core_count);
			
      case FINITE_BUFFER_NETWORK_FLIP_ATAC:
         return FiniteBufferNetworkModelFlipAtac::computeCoreCountConstraints(core_count);
         
      default:
         fprintf(stderr, "Unrecognized network type(%u)\n", network_type);
         assert(false);
         return make_pair(false,-1);
   }
}

pair<bool, vector<core_id_t> > 
NetworkModel::computeMemoryControllerPositions(UInt32 network_type, SInt32 num_memory_controllers)
{
   switch(network_type)
   {
      case NETWORK_MAGIC:
      case NETWORK_EMESH_HOP_COUNTER:
         {
            SInt32 core_count = (SInt32) Config::getSingleton()->getTotalCores();
            SInt32 spacing_between_memory_controllers = core_count / num_memory_controllers;
            vector<core_id_t> core_list_with_memory_controllers;
            for (core_id_t i = 0; i < num_memory_controllers; i++)
            {
               assert((i*spacing_between_memory_controllers) < core_count);
               core_list_with_memory_controllers.push_back(i * spacing_between_memory_controllers);
            }
            
            return make_pair(false, core_list_with_memory_controllers);
         }

      case FINITE_BUFFER_NETWORK_EMESH:
         return FiniteBufferNetworkModelEMesh::computeMemoryControllerPositions(num_memory_controllers);
      
      case FINITE_BUFFER_NETWORK_ATAC:
         return FiniteBufferNetworkModelAtac::computeMemoryControllerPositions(num_memory_controllers);

      case FINITE_BUFFER_NETWORK_CLOS:
            return FiniteBufferNetworkModelClos::computeMemoryControllerPositions(num_memory_controllers);

      case FINITE_BUFFER_NETWORK_FLIP_ATAC:
         return FiniteBufferNetworkModelFlipAtac::computeMemoryControllerPositions(num_memory_controllers); 
         
      default:
         LOG_PRINT_ERROR("Unrecognized network type(%u)", network_type);
         return make_pair(false, vector<core_id_t>());
   }
}

core_id_t
NetworkModel::getRequester(const NetPacket* packet)
{
   // USER network -- (packet.sender)
   // SHARED_MEM network -- (getRequester(packet))
   // SYSTEM network -- INVALID_CORE_ID
   if ((_network_id == STATIC_NETWORK_USER_1) || (_network_id == STATIC_NETWORK_USER_2))
      return packet->sender;
   else if ((_network_id == STATIC_NETWORK_MEMORY_1) || (_network_id == STATIC_NETWORK_MEMORY_2))
      return _network->getCore()->getMemoryManager()->getShmemRequester(packet->data);
   else // (_network_id == STATIC_NETWORK_SYSTEM)
      return packet->sender; 
}

bool
NetworkModel::isModeled(const NetPacket* packet)
{
   // USER network -- (true)
   // SHARED_MEM network -- (true)
   // SYSTEM network -- (false)
   return (_network_id == STATIC_NETWORK_SYSTEM) ? false : true;
}

SInt32
NetworkModel::getModeledLength(const NetPacket* packet)
{
   SInt32 header_size = 1 + 2 * Config::getSingleton()->getCoreIDLength() + 2;
   if ((packet->type == SHARED_MEM_1) || (packet->type == SHARED_MEM_2))
   {
      // packet_type + sender + receiver + length + shmem_msg.size()
      // 1 byte for packet_type
      // log2(core_id) for sender and receiver
      // 2 bytes for packet length
      SInt32 data_size = _network->getCore()->getMemoryManager()->getModeledLength(packet->data);
      return header_size + data_size;
   }
   else
   {
      return header_size + packet->length;
   }
}

SInt32
NetworkModel::computeSerializationLatency(const NetPacket* raw_packet)
{
   assert(raw_packet->is_raw);
   if (_flit_width == INFINITE_BANDWIDTH)
      return 1;

   SInt32 packet_length = getModeledLength(raw_packet);
   return (SInt32) ceil((float) (packet_length * 8) / _flit_width);
}

void
NetworkModel::initializePerformanceCounters()
{
   // Send
   _total_packets_sent = 0;
   _total_flits_sent = 0;
   _total_bytes_sent = 0;
   // Broadcasted
   _total_packets_broadcasted = 0;
   _total_flits_broadcasted = 0;
   _total_bytes_broadcasted = 0;
   // Received
   _total_packets_received = 0;
   _total_flits_received = 0;
   _total_bytes_received = 0;
   // Delay Counters
   _total_packet_latency = 0;
   _total_contention_delay = 0;
   // Throughput Counters
   _last_packet_send_time = 0;
   _last_packet_recv_time = 0;
}

void
NetworkModel::updatePacketSendStatistics(const NetPacket* raw_packet)
{
   SInt32 num_flits = computeSerializationLatency(raw_packet);
   SInt32 num_bytes = getModeledLength(raw_packet);

   // Sent
   _total_packets_sent ++;
   _total_flits_sent += num_flits;
   _total_bytes_sent += num_bytes;

   // Broadcasts
   if (raw_packet->receiver == NetPacket::BROADCAST)
   {
      _total_packets_broadcasted ++;
      _total_flits_broadcasted += num_flits;
      _total_bytes_broadcasted += num_bytes;
   }

   // Send Time
   // assert(_last_packet_send_time <= raw_packet->start_time);
   _last_packet_send_time = raw_packet->start_time;
}

void
NetworkModel::updatePacketReceiveStatistics(const NetPacket* raw_packet, SInt32 zero_load_delay)
{
   UInt64 packet_latency = raw_packet->time - raw_packet->start_time;
   UInt64 contention_delay = packet_latency - zero_load_delay;
   
   LOG_ASSERT_ERROR( ((UInt64)zero_load_delay) <= packet_latency,
                    "[Sender(%i), Receiver(%i), Curr Core(%i)] : Zero Load Delay(%i) > Packet Latency(%llu)",
                    raw_packet->sender, raw_packet->receiver, _core_id, zero_load_delay, packet_latency);
   

   SInt32 num_flits = computeSerializationLatency(raw_packet);
   SInt32 num_bytes = getModeledLength(raw_packet);
  
   // Received 
   _total_packets_received ++;
   _total_flits_received += num_flits;
   _total_bytes_received += num_bytes;
   
   // Performance Counters
   _total_packet_latency += packet_latency;
   _total_contention_delay += contention_delay;
   
   // Recv Time
   assert(_last_packet_recv_time <= raw_packet->time);
   _last_packet_recv_time = raw_packet->time;
}

void
NetworkModel::outputSummary(ostream& out)
{
   // Sent
   out << "    Total Packets Sent: " << _total_packets_sent << endl;
   out << "    Total Flits Sent: " << _total_flits_sent << endl;
   out << "    Total Bytes Sent: " << _total_bytes_sent << endl;
   // Broadcasted
   out << "    Total Packets Broadcasted: " << _total_packets_broadcasted << endl;
   out << "    Total Flits Broadcasted: " << _total_flits_broadcasted << endl;
   out << "    Total Bytes Broadcasted: " << _total_bytes_broadcasted << endl;
   // Received
   out << "    Total Packets Received: " << _total_packets_received << endl;
   out << "    Total Flits Received: " << _total_flits_received << endl;
   out << "    Total Bytes Received: " << _total_bytes_received << endl;
   
   // Delay Counters
   if (_total_packets_received > 0)
   {
      UInt64 total_contention_delay_in_ns = convertCycleCount(_total_contention_delay, getFrequency(), 1.0);
      UInt64 total_packet_latency_in_ns = convertCycleCount(_total_packet_latency, getFrequency(), 1.0);

      out << "    Average Packet Length (in bytes): " << 
         ((float) _total_bytes_received) / _total_packets_received << endl;
      
      out << "    Average Contention Delay (in clock-cycles): " << 
         ((double) _total_contention_delay) / _total_packets_received << endl;
      out << "    Average Contention Delay (in ns): " << 
         ((double) total_contention_delay_in_ns) / _total_packets_received << endl;
      
      out << "    Average Packet Latency (in clock-cycles): " <<
         ((double) _total_packet_latency) / _total_packets_received << endl;
      out << "    Average Packet Latency (in ns): " <<
         ((double) total_packet_latency_in_ns) / _total_packets_received << endl;
   }
   else
   {
      out << "    Average Packet Length (in bytes): 0" << endl;
      out << "    Average Contention Delay (in clock-cycles): 0" << endl;
      out << "    Average Contention Delay (in ns): 0" << endl;
      out << "    Average Packet Latency (in clock-cycles): 0" << endl;
      out << "    Average Packet Latency (in ns): 0" << endl;
   }

   // Offered Throughput
   if (_total_packets_sent > 0)
   {
      out << "    Offered Throughput (in flits/clock-cycle): " <<
         ((double) _total_flits_sent) / _last_packet_send_time << endl;
      out << "    Offered Broadcast Throughput (in flits/clock-cycle): " <<
         ((double) _total_flits_broadcasted) / _last_packet_send_time << endl;
   }
   else
   {
      out << "    Offered Throughput (in flits/clock-cycle): 0" << endl;
      out << "    Offered Broadcast Throughput (in flits/clock-cycle): 0" << endl;
   }

   // Sustained Throughput
   if (_total_flits_received > 0)
   {
      out << "    Sustained Throughput (in flits/clock-cycle): " <<
         ((double) _total_flits_received) / _last_packet_recv_time << endl;
   }
   else
   {
      out << "    Sustained Throughput (in flits/clock-cycle): 0" << endl;
   }
}
