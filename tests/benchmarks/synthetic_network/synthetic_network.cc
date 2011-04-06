#include <stdio.h>
#include <time.h>
#include <cmath>
#include "simulator.h"
#include "core_manager.h"
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

CoreSpVars* _core_sp_vars;
UInt64 _quantum = 1000;
Semaphore _semaphore;

UInt32 EVENT_NET_SEND = 100;
UInt32 EVENT_PUSH_FIRST_EVENTS = 102;

#include "traffic_generators.h"

int main(int argc, char* argv[])
{
   printf("\n[SYNTHETIC NETWORK BENCHMARK]: Starting Test\n\n");

   debug_printf("CarbonStartSim() start\n");
   CarbonStartSim(argc, argv);
   debug_printf("CarbonStartSim() end\n");

   Simulator::enablePerformanceModelsInCurrentProcess();
   debug_printf("Enabled Performance Models\n");
   
   // Read Command Line Arguments
   for (SInt32 i = 1; i < argc-1; i += 2)
   {
      if (string(argv[i]) == "-p")
         _traffic_pattern_type = parseTrafficPattern(string(argv[i+1]));
      else if (string(argv[i]) == "-l")
         _offered_load = (double) atof(argv[i+1]);
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

   _num_cores = (SInt32) Config::getSingleton()->getApplicationCores();
   debug_printf("Num Application Cores(%i)\n", _num_cores);

   // Initialize Core Specific Variables
   initializeCoreSpVars();

   // Register (NetSend, NetRecv, PushFirstEvents) Events
   Event::registerHandler(EVENT_NET_SEND, processNetSendEvent);
   registerRecvdPacketHandler();
   Event::registerHandler(EVENT_PUSH_FIRST_EVENTS, pushFirstEvents);
   
   // Push First Event
   UnstructuredBuffer event_args;
   Event* push_first_events = new Event((Event::Type) EVENT_PUSH_FIRST_EVENTS, 0 /* time */, event_args);
   Event::processInOrder(push_first_events, 0 /* core_id */, EventQueue::ORDERED);

   // Wait till all packets are sent and received
   waitForCompletion();
   
   // Unregister events
   Event::unregisterHandler(EVENT_PUSH_FIRST_EVENTS);
   unregisterRecvdPacketHandler();
   Event::unregisterHandler(EVENT_NET_SEND);

   // Delete Core Specific Variables
   deinitializeCoreSpVars();

   Simulator::disablePerformanceModelsInCurrentProcess();
   
   printf("\n[SYNTHETIC NETWORK BENCHMARK]: Finished Test successfully\n\n");

   CarbonStopSim();
 
   return 0;
}

void registerRecvdPacketHandler()
{
   for (SInt32 i = 0; i < _num_cores; i++)
   {
      Core* core = Sim()->getCoreManager()->getCoreFromID(i);
      core->getNetwork()->registerCallback(_packet_type, processRecvdPacket, core);
   }
}

void unregisterRecvdPacketHandler()
{
   for (SInt32 i = 0; i < _num_cores; i++)
   {
      Core* core = Sim()->getCoreManager()->getCoreFromID(i);
      core->getNetwork()->unregisterCallback(_packet_type);
   }
}

void waitForCompletion()
{
   for (SInt32 i = 0; i < (2 * _num_cores); i++)
      _semaphore.wait();
}

void printHelpMessage()
{
   fprintf(stderr, "[Usage]: ./synthetic_network_traffic_generator -p <arg1> -l <arg2> -s <arg3> -N <arg4>\n");
   fprintf(stderr, "where <arg1> = Network Traffic Pattern Type (uniform_random, bit_complement, shuffle, transpose, tornado, nearest_neighbor) (default uniform_random)\n");
   fprintf(stderr, " and  <arg2> = Number of Packets injected into the Network per Core per Cycle (default 0.1)\n");
   fprintf(stderr, " and  <arg3> = Size of each Packet in Bytes (default 8)\n");
   fprintf(stderr, " and  <arg4> = Total Number of Packets injected into the Network per Core (default 10000)\n");
}

void initializeCoreSpVars()
{
   _core_sp_vars = new CoreSpVars[_num_cores];
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

      RandNum* rand_num = new RandNum(0,1 /* range */, i /* seed */);

      // Populate the core specific structure
      _core_sp_vars[i].init(rand_num, send_vec, receive_vec);
   }
}

void deinitializeCoreSpVars()
{
   delete [] _core_sp_vars;
}

void processNetSendEvent(Event* event)
{
   assert(event->getType() == (Event::Type) EVENT_NET_SEND);
   
   Core* core;
   UnstructuredBuffer& event_args = event->getArgs();
   event_args >> core;

   RandNum* rand_num = _core_sp_vars[core->getId()]._rand_num;
   UInt64& total_packets_sent = _core_sp_vars[core->getId()]._total_packets_sent;
   vector<int>& send_vec = _core_sp_vars[core->getId()]._send_vec;
   UInt64& last_packet_time = _core_sp_vars[core->getId()]._last_packet_time;
   
   for (UInt64 time = event->getTime(); time < (event->getTime() + _quantum); time++)
   {
      if ((total_packets_sent < _total_packets) && (canSendPacket(_offered_load, rand_num)))
      {
         if ((core->getId() == 0) && ((total_packets_sent % 100) == 0))
         {
            printf("Core(0) sending packet(%llu), Time(%llu)\n", \
                  (long long unsigned int) total_packets_sent, (long long unsigned int) event->getTime());
         }

         // Send a packet to its destination core
         Byte data[_packet_size];
         SInt32 receiver = send_vec[total_packets_sent % send_vec.size()];
         
         last_packet_time = getMax<UInt64>(last_packet_time, time);
         NetPacket net_packet(last_packet_time, _packet_type, core->getId(), receiver, _packet_size, data);
         core->getNetwork()->netSend(net_packet);
        
         last_packet_time += computeNumFlits(core->getNetwork()->getModeledLength(net_packet)); 
         total_packets_sent ++;
      }
   }

   if (total_packets_sent < _total_packets)
   {
      pushEvent(event->getTime() + _quantum, core);
   }
   else // (total_packets_sent == _total_packets)
   {
      _semaphore.signal();
   }
}

void processRecvdPacket(void* obj, NetPacket net_packet)
{
   Core* recv_core = (Core*) obj;
   core_id_t receiver = recv_core->getId();
   assert((receiver >= 0) && (receiver < _num_cores));
   
   UInt64& total_packets_received = _core_sp_vars[receiver]._total_packets_received;
   
   if ((recv_core->getId() == 0) && ((total_packets_received % 100) == 0))
   {
      printf("Core(0) receiving packet(%llu), Time(%llu)\n", \
            (long long unsigned int) total_packets_received, (long long unsigned int) net_packet.time);
   }

   total_packets_received ++;

   if (total_packets_received == _total_packets)
   {
      _semaphore.signal();
   }
}

void pushFirstEvents(Event* event)
{
   debug_printf("pushFirstEvents\n");
   for (SInt32 i = 0; i < _num_cores; i++)
   {
      Core* ith_core = Sim()->getCoreManager()->getCoreFromID(i);
      // Push the first events
      pushEvent(0 /* time */, ith_core);
   }
}

void pushEvent(UInt64 time, Core* core)
{
   UnstructuredBuffer event_args;
   event_args << core;
   Event* event = new Event((Event::Type) EVENT_NET_SEND, time, event_args);
   Event::processInOrder(event, core->getId(), EventQueue::ORDERED);
}

bool canSendPacket(double offered_load, RandNum* rand_num)
{
   return (rand_num->next() < offered_load); 
}

SInt32 computeNumFlits(SInt32 length)
{
   SInt32 num_flits = (SInt32) ceil((float) (length * 8) / _flit_width);
   return num_flits;
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
