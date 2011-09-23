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
// Size of each Unicast Packet in Bytes
SInt32 _unicast_packet_size = 8;
// Size of each Broadcast Packet in Bytes
SInt32 _broadcast_packet_size = 8;
// Total number of packets injected into the network per core
UInt64 _total_packets = 10000;
// Number of cores in the network
SInt32 _num_cores;

// Type of each packet (so as to send on USER_2 network)
PacketType _packet_type = USER_2;

vector<SyntheticCore*> _synthetic_core_list;
UInt64 _quantum = 100;

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
      else if (string(argv[i]) == "-us")
         _unicast_packet_size = (SInt32) atoi(argv[i+1]);
      else if (string(argv[i]) == "-bs")
         _broadcast_packet_size = (SInt32) atoi(argv[i+1]);
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

   // Initialize Synthetic Core Objs
   initializeSyntheticCores();

   // Register (PacketSent, PacketRecv, StartSimulation) Events
   registerNetPacketInjectorExitHandler();
   Event::registerHandler(EVENT_NET_SEND, processNetSendEvent);
   registerAsyncNetRecvHandler();
   Event::registerHandler(EVENT_START_SIMULATION, processStartSimulationEvent);
   
   // Push First Event
   Event* start_simulation_event = new Event((Event::Type) EVENT_START_SIMULATION, 0 /* time */);
   Event::processInOrder(start_simulation_event, 0 /* core_id */, EventQueue::ORDERED);

   // Wait till all packets are sent and received
   waitForCompletion();
   
   // Unregister events
   Event::unregisterHandler(EVENT_START_SIMULATION);
   unregisterAsyncNetRecvHandler();
   Event::unregisterHandler(EVENT_NET_SEND);
   unregisterNetPacketInjectorExitHandler();

   // Delete Synthetic Core Objs
   // deinitializeSyntheticCores();

   Simulator::__disablePerformanceModels();
   
   printf("\n[SYNTHETIC NETWORK BENCHMARK]: Finished Test successfully\n\n");

   CarbonStopSim();
 
   return 0;
}

void outputSummary(void* callback_obj, ostream& out)
{
   SyntheticCore* synthetic_core = (SyntheticCore*) callback_obj;
   synthetic_core->outputSummary(out);
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
      _synthetic_core_list[i]->netSend(0 /* time */);
   }
}

void netPacketInjectorExitCallback(void* obj, UInt64 time)
{
   // Push NetSend Event
   // Get the synthetic core ptr
   SyntheticCore* synthetic_core = (SyntheticCore*) obj;

   // Construct the event buffer
   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) <<synthetic_core;
   
   // Push the event on the event queue
   Event* net_send_event = new Event((Event::Type) EVENT_NET_SEND, time /* time */, event_args /* arguments */);
   Event::processInOrder(net_send_event, synthetic_core->getCore()->getId() /* core_id */, EventQueue::ORDERED);
}

void processNetSendEvent(Event* event)
{
   SyntheticCore* synthetic_core;
   UnstructuredBuffer* event_args = event->getArgs();
   (*event_args) >> synthetic_core;

   // Process the net send event
   synthetic_core->netSend(event->getTime());
}

void asyncNetRecvCallback(void* obj, NetPacket net_packet)
{
   SyntheticCore* synthetic_core = (SyntheticCore*) obj;
   synthetic_core->netRecv(net_packet);
}

void registerNetPacketInjectorExitHandler()
{
   for (SInt32 i = 0; i < _num_cores; i++)
   {
      Core* core = Sim()->getCoreManager()->getCoreFromID(i);
      FiniteBufferNetworkModel* model = (FiniteBufferNetworkModel*) core->getNetwork()->getNetworkModelFromPacketType(_packet_type);
      model->registerNetPacketInjectorExitCallback(netPacketInjectorExitCallback, _synthetic_core_list[i]);
   }
}

void registerAsyncNetRecvHandler()
{
   for (SInt32 i = 0; i < _num_cores; i++)
   {
      Core* core = Sim()->getCoreManager()->getCoreFromID(i);
      core->getNetwork()->registerAsyncRecvCallback(_packet_type, asyncNetRecvCallback, _synthetic_core_list[i]);
   }
}

void unregisterNetPacketInjectorExitHandler()
{
   for (SInt32 i = 0; i < _num_cores; i++)
   {
      Core* core = Sim()->getCoreManager()->getCoreFromID(i);
      FiniteBufferNetworkModel* model = (FiniteBufferNetworkModel*) core->getNetwork()->getNetworkModelFromPacketType(_packet_type);
      model->unregisterNetPacketInjectorExitCallback();
   }
}

void unregisterAsyncNetRecvHandler()
{
   for (SInt32 i = 0; i < _num_cores; i++)
   {
      Core* core = Sim()->getCoreManager()->getCoreFromID(i);
      core->getNetwork()->unregisterAsyncRecvCallback(_packet_type);
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
   fprintf(stderr, "[Usage]: ./synthetic_network_traffic_generator -p <arg1> -l <arg2> -b <arg3> -us <arg4> -bs <arg5> -N <arg6>\n");
   fprintf(stderr, "where <arg1> = Network Traffic Pattern Type (uniform_random, bit_complement, shuffle, transpose, tornado, nearest_neighbor) (default uniform_random)\n");
   fprintf(stderr, " and  <arg2> = Number of Packets injected into the Network per Core per Cycle (default 0.1)\n");
   fprintf(stderr, " and  <arg3> = Fraction of Broadcasts among Packets sent (default 0.0)\n");
   fprintf(stderr, " and  <arg4> = Payload Size of each Unicast Packet in Bytes (default 8)\n");
   fprintf(stderr, " and  <arg5> = Payload Size of each Broadcast Packet in Bytes (default 8)\n");
   fprintf(stderr, " and  <arg6> = Total Number of Packets injected into the Network per Core (default 10000)\n");
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
   , _total_flits_sent(0)
   , _total_packets_received(0)
   , _total_flits_received(0)
   , _last_packet_send_time(0)
   , _last_packet_recv_time(0)
{
   _inject_packet_rand_num = new RandNum(0, 1, _core->getId() /* seed */);
   _broadcast_packet_rand_num = new RandNum(0, 1, _core->getId() /* seed */);

   NetworkModel* network_model = _core->getNetwork()->getNetworkModelFromPacketType(_packet_type);
   LOG_ASSERT_ERROR(network_model->isFiniteBuffer(), "Only Finite Buffer Network Models can be used in the [network/user_model_2] configuration option for running this benchmarks");
   _network_model = (FiniteBufferNetworkModel*) network_model;

   // Output Summary
   _core->registerExternalOutputSummaryCallback(::outputSummary, this);
}

SyntheticCore::~SyntheticCore()
{
   delete _inject_packet_rand_num;
   delete _broadcast_packet_rand_num;

   // Output Summary
   _core->unregisterExternalOutputSummaryCallback(::outputSummary);
}

void SyntheticCore::netSend(UInt64 net_packet_injector_exit_time)
{
   LOG_PRINT("Synthetic Core(%i): netSend(%llu)", _core->getId(), net_packet_injector_exit_time);
   LOG_PRINT("Total Packets Sent(%llu), Total Packets(%llu), Last Packet Send Time(%llu)",
         _total_packets_sent, _total_packets, _last_packet_send_time);

   if (_total_packets_sent == _total_packets)
   {
      LOG_PRINT("Synthetic Core(%i): All packet sent", _core->getId());
      return;
   }

   assert(net_packet_injector_exit_time >= _last_packet_send_time);

   for (UInt64 time = _last_packet_send_time + 1 ; ; time ++)
   {
      if (canInjectPacket())
      {
         logNetSend(time);
         
         // Send a packet to its destination core
         SInt32 receiver = (isBroadcastPacket()) ? NetPacket::BROADCAST :
                                                   _send_vec[_total_packets_sent % _send_vec.size()];
         SInt32 packet_size = (receiver == NetPacket::BROADCAST) ? _broadcast_packet_size : _unicast_packet_size;

         _last_packet_send_time = time;
         
         // Construct packet
         Byte data[packet_size];
         UInt64 send_time = max<UInt64>(_last_packet_send_time, net_packet_injector_exit_time);
         NetPacket net_packet(send_time, _packet_type, _core->getId(), receiver, packet_size, data);

         // Event Counters
         _total_packets_sent ++;
         _total_flits_sent += _network_model->computeSerializationLatency(&net_packet);

         // Send out packet
         LOG_PRINT("Sending Out Packet: Time(%llu)", _last_packet_send_time);
         _core->getNetwork()->netSend(net_packet, _last_packet_send_time);

         break;
      }
   }
}

void SyntheticCore::netRecv(const NetPacket& net_packet)
{
   LOG_PRINT("Synthetic Core(%i): netRecv(%llu)", _core->getId(), net_packet.time);

   logNetRecv(net_packet.time);

   // Event Counters
   LOG_ASSERT_ERROR(_last_packet_recv_time <= net_packet.time, "Last Recv Packet Time(%llu), Curr Packet Time(%llu)",
                    _last_packet_recv_time, net_packet.time);
   _last_packet_recv_time = net_packet.time;
   _total_packets_received ++;
   _total_flits_received += _network_model->computeSerializationLatency(&net_packet);
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

void SyntheticCore::outputSummary(ostream& out)
{
   out << "Synthetic Core Summary: " << endl;
   out << "    Total Flits Sent: " << _total_flits_sent << endl;
   out << "    Send Completion Time: " << _last_packet_send_time << endl;
   out << "    Total Flits Received: " << _total_flits_received << endl;
   out << "    Recv Completion Time: " << _last_packet_recv_time << endl;
   float offered_throughput = ((float) _total_flits_sent) / _last_packet_send_time;
   out << "    Offered Throughput: " << offered_throughput << endl;
   float sustained_throughput = ((float) _total_flits_received) / _last_packet_recv_time;
   out << "    Sustained Throughput: " << sustained_throughput << endl;
}
