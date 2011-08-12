#include <stdio.h>
#include <time.h>
#include <cmath>
#include "simulator.h"
#include "core_manager.h"
#include "event_manager.h"
#include "config.h"
#include "core.h"
#include "network.h"
#include "utils.h"
#include "semaphore.h"
#include "carbon_user.h"
#include "synthetic_network.h"

// #define DEBUG 1

// Network Traffic Pattern Type
NetworkTrafficType _traffic_pattern_type = UNIFORM_RANDOM;
// Number of packets injected per core per cycle
double _offered_load = 0.1;
// Fraction of Broadcasts among injected packets
double _fraction_broadcasts = 0.0;
// Size of each Packet in Bytes
SInt32 _packet_size = 8;
// Total number of packets injected into the network per core
UInt64 _total_packets = 10000;
// Number of cores in the network
SInt32 _num_cores;

// Type of each packet (so as to send on USER_2 network)
PacketType _packet_type = USER_2;
// FIXME: Hard-coded for now (in bits), Change Later
SInt32 _flit_width = 64;

vector<SyntheticCore*> _synthetic_core_list;
UInt64 _quantum = 1000;

UInt32 EVENT_NET_SEND = 100;
UInt32 EVENT_START_SIMULATION = 102;

#include "traffic_generators.h"

int main(int argc, char* argv[])
{
   printf("\n[SYNTHETIC NETWORK BENCHMARK]: Starting Test\n\n");

   CarbonStartSim(argc, argv);
   debug_printf("Executed CarbonStartSim()\n");

   Simulator::__enablePerformanceModels();
   debug_printf("Enabled Performance Models\n");
   
   // Read Command Line Arguments
   for (SInt32 i = 1; i < argc-1; i += 2)
   {
      if (string(argv[i]) == "-p")
         _traffic_pattern_type = parseTrafficPattern(string(argv[i+1]));
      else if (string(argv[i]) == "-l")
         _offered_load = (double) atof(argv[i+1]);
      else if (string(argv[i]) == "-b")
         _fraction_broadcasts = (double) atof(argv[i+1]);
      else if (string(argv[i]) == "-s")
         _packet_size = (SInt32) atoi(argv[i+1]);
      else if (string(argv[i]) == "-N")
         _total_packets = (UInt64) atoi(argv[i+1]);
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

   debug_printf("Finished parsing command line arguments\n");

   _num_cores = (SInt32) Config::getSingleton()->getTotalCores();
   debug_printf("Num Application Cores(%i)\n", _num_cores);

   // Initialize Core Specific Variables
   initializeSyntheticCores();

   // Register (NetSend, NetRecv, PushFirstEvents) Events
   Event::registerHandler(EVENT_NET_SEND, processNetSendEvent);
   registerNetRecvHandler();
   Event::registerHandler(EVENT_START_SIMULATION, processStartSimulationEvent);
   
   // Push First Event
   Event* start_simulation_event = new Event((Event::Type) EVENT_START_SIMULATION, 0 /* time */);
   Event::processInOrder(start_simulation_event, 0 /* core_id */, EventQueue::ORDERED);

   // Wait till all packets are sent and received
   waitForCompletion();
   
   // Unregister events
   Event::unregisterHandler(EVENT_START_SIMULATION);
   unregisterNetRecvHandler();
   Event::unregisterHandler(EVENT_NET_SEND);

   // Delete Core Specific Variables
   deinitializeSyntheticCores();

   Simulator::__disablePerformanceModels();
   
   printf("\n[SYNTHETIC NETWORK BENCHMARK]: Finished Test successfully\n\n");

   CarbonStopSim();
 
   return 0;
}

void initializeSyntheticCores()
{
   _synthetic_core_list.resize(_num_cores);
   for (SInt32 i = 0; i < _num_cores; i++)
   {
      vector<int> send_vec;
      vector<int> receive_vec;
      
      // Generate the Network Traffic
      switch (_traffic_pattern_type)
      {
         case UNIFORM_RANDOM:
            uniformRandomTrafficGenerator(i, send_vec, receive_vec);
            break;
         case BIT_COMPLEMENT:
            bitComplementTrafficGenerator(i, send_vec, receive_vec);
            break;
         case SHUFFLE:
            shuffleTrafficGenerator(i, send_vec, receive_vec);
            break;
         case TRANSPOSE:
            transposeTrafficGenerator(i, send_vec, receive_vec);
            break;
         case TORNADO:
            tornadoTrafficGenerator(i, send_vec, receive_vec);
            break;
         case NEAREST_NEIGHBOR:
            nearestNeighborTrafficGenerator(i, send_vec, receive_vec);
            break;
         default:
            assert(false);
            break;
      }

      // Populate the core specific structure
      Core* core = Sim()->getCoreManager()->getCoreFromID(i);
      _synthetic_core_list[i] = new SyntheticCore(core, send_vec, receive_vec);
   }
}

void deinitializeSyntheticCores()
{
   for (SInt32 i = 0; i < _num_cores; i++)
      delete _synthetic_core_list[i];
   _synthetic_core_list.clear();
}

void processStartSimulationEvent(Event* event)
{
   debug_printf("processStartSimulationEvent\n");
   for (SInt32 i = 0; i < _num_cores; i++)
   {
      // Push the first events
      _synthetic_core_list[i]->pushNetSendEvent(0 /* time */);
   }
}

void processNetSendEvent(Event* event)
{
   assert(event->getType() == (Event::Type) EVENT_NET_SEND);
   
   Core* core;
   UnstructuredBuffer* event_args = event->getArgs();
   (*event_args) >> core;

   _synthetic_core_list[core->getId()]->processNetSendEvent(event);
}

void processNetRecvEvent(void* obj, NetPacket net_packet)
{
   Core* recv_core = (Core*) obj;
   core_id_t receiver = recv_core->getId();
   assert((receiver >= 0) && (receiver < _num_cores));

   _synthetic_core_list[receiver]->processNetRecvEvent(net_packet);
}

void registerNetRecvHandler()
{
   for (SInt32 i = 0; i < _num_cores; i++)
   {
      Core* core = Sim()->getCoreManager()->getCoreFromID(i);
      core->getNetwork()->registerCallback(_packet_type, processNetRecvEvent, core);
   }
}

void unregisterNetRecvHandler()
{
   for (SInt32 i = 0; i < _num_cores; i++)
   {
      Core* core = Sim()->getCoreManager()->getCoreFromID(i);
      core->getNetwork()->unregisterCallback(_packet_type);
   }
}

void waitForCompletion()
{
   // Sleep in a loop for 100 milli-sec each. Wake up and see if done
   while (1)
   {
      usleep(100000);
      if (! Sim()->getEventManager()->hasEventsPending())
         break;
   }
}

void printHelpMessage()
{
   fprintf(stderr, "[Usage]: ./synthetic_network_traffic_generator -p <arg1> -l <arg2> -b <arg3> -s <arg4> -N <arg5>\n");
   fprintf(stderr, "where <arg1> = Network Traffic Pattern Type (uniform_random, bit_complement, shuffle, transpose, tornado, nearest_neighbor) (default uniform_random)\n");
   fprintf(stderr, " and  <arg2> = Number of Packets injected into the Network per Core per Cycle (default 0.1)\n");
   fprintf(stderr, " and  <arg3> = Fraction of Broadcasts among Packets sent (default 0.0)\n");
   fprintf(stderr, " and  <arg4> = Size of each Packet in Bytes (default 8)\n");
   fprintf(stderr, " and  <arg5> = Total Number of Packets injected into the Network per Core (default 10000)\n");
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

SyntheticCore::SyntheticCore(Core* core, const vector<int>& send_vec, const vector<int>& receive_vec)
   : _core(core)
   , _send_vec(send_vec)
   , _receive_vec(receive_vec)
   , _total_packets_sent(0)
   , _total_packets_received(0)
   , _last_packet_time(0)
{
   _inject_packet_rand_num = new RandNum(0, 1, _core->getId() /* seed */);
   _broadcast_packet_rand_num = new RandNum(0, 1, _core->getId() /* seed */);
}

SyntheticCore::~SyntheticCore()
{
   delete _inject_packet_rand_num;
   delete _broadcast_packet_rand_num;
}

void SyntheticCore::processNetSendEvent(Event* event)
{
   for (UInt64 time = event->getTime(); time < (event->getTime() + _quantum); time++)
   {
      if (_total_packets_sent >= _total_packets)
         break;

      if (canInjectPacket())
      {
         logNetSend(time);

         SInt32 receiver = (isBroadcastPacket()) ? NetPacket::BROADCAST :
                                                   _send_vec[_total_packets_sent % _send_vec.size()];
         // Send a packet to its destination core
         Byte data[_packet_size];
         
         _last_packet_time = getMax<UInt64>(_last_packet_time, time);
         NetPacket net_packet(_last_packet_time, _packet_type, _core->getId(), receiver, _packet_size, data);
         _core->getNetwork()->netSend(net_packet);
        
         _last_packet_time += computeNumFlits(_core->getNetwork()->getModeledLength(net_packet)); 
         _total_packets_sent ++;
      }
   }

   if (_total_packets_sent < _total_packets)
   {
      pushNetSendEvent(event->getTime() + _quantum);
   }
}

void SyntheticCore::processNetRecvEvent(const NetPacket& net_packet)
{
   logNetRecv(net_packet.time);

   _total_packets_received ++;
}

void SyntheticCore::pushNetSendEvent(UInt64 time)
{
   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) << _core;
   Event* event = new Event((Event::Type) EVENT_NET_SEND, time, event_args);
   Event::processInOrder(event, _core->getId(), EventQueue::ORDERED);
}

bool SyntheticCore::canInjectPacket()
{
   return (_inject_packet_rand_num->next() < _offered_load);
}

bool SyntheticCore::isBroadcastPacket()
{
   return (_broadcast_packet_rand_num->next() < _fraction_broadcasts);
}

void SyntheticCore::logNetSend(UInt64 time)
{
   if ((_core->getId() == 0) && ((_total_packets_sent % 100) == 0))
   {
      debug_printf("Core(0) sending packet(%llu), Time(%llu)\n",
                   (long long unsigned int) _total_packets_sent, (long long unsigned int) time);
   }
}

void SyntheticCore::logNetRecv(UInt64 time)
{
   if ((_core->getId() == 0) && ((_total_packets_received % 100) == 0))
   {
      debug_printf("Core(0) receiving packet(%llu), Time(%llu)\n",
                   (long long unsigned int) _total_packets_received, (long long unsigned int) time);
   }
}

SInt32 SyntheticCore::computeNumFlits(SInt32 length)
{
   SInt32 num_flits = (SInt32) ceil((float) (length * 8) / _flit_width);
   return num_flits;
}
