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
// Warmup and Measurement Phases
SInt32 _num_cores_with_warmup_phase_completed = 0;
SInt32 _num_cores_with_measurement_phase_completed = 0;

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
   log("Executed CarbonStartSim()");

   Simulator::__enablePerformanceModels();
   log("Enabled Performance Models");
   
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

   log("Finished parsing command line arguments");

   _num_cores = (SInt32) Config::getSingleton()->getTotalCores();
   log("Num Application Cores(%i)", _num_cores);

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
   log("processStartSimulationEvent");
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

void logimp(const char* fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);

   vfprintf(stderr, fmt, ap);
   fprintf(stderr, "\n");
   va_end(ap);
}

void log(const char* fmt, ...)
{
#ifdef DEBUG
   va_list ap;
   va_start(ap, fmt);

   vfprintf(stderr, fmt, ap);
   fprintf(stderr, "\n");
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
   , _total_packet_latency(0)
   , _last_packet_send_time(0)
   , _last_packet_recv_time(0)
   , _warmup_phase_enabled(false)
   , _measurement_phase_enabled(false)
   , _measurement_phase_start_time(0)
   , _measurement_phase_end_time(0)
   , _cooldown_phase_enabled(false)
{
   // Random Numbers
   _inject_packet_rand_num = new RandNum(0, 1, _core->getId() /* seed */);
   _broadcast_packet_rand_num = new RandNum(0, 1, _core->getId() /* seed */);

   NetworkModel* network_model = _core->getNetwork()->getNetworkModelFromPacketType(_packet_type);
   LOG_ASSERT_ERROR(network_model->isFiniteBuffer(), "Only Finite Buffer Network Models can be used in the [network/user_model_2] configuration option for running this benchmark");
   _network_model = (FiniteBufferNetworkModel*) network_model;

   // Start Warmup Phase
   startWarmupPhase();

   // Output Summary
   _core->registerExternalOutputSummaryCallback(::outputSummary, this);
}

SyntheticCore::~SyntheticCore()
{
   // End Cooldown Phase
   endCooldownPhase();

   // Random Numbers
   delete _inject_packet_rand_num;
   delete _broadcast_packet_rand_num;

   // Output Summary
   _core->unregisterExternalOutputSummaryCallback(::outputSummary);
}

void SyntheticCore::netSend(UInt64 net_packet_injector_exit_time)
{
   LOG_PRINT("Synthetic Core(%i): netSend(%llu)", _core->getId(), net_packet_injector_exit_time);
   LOG_PRINT("Total Packets Sent(%llu), Last Packet Send Time(%llu)", _total_packets_sent, _last_packet_send_time);

   if (_cooldown_phase_enabled)
   {
      log("Synthetic Core(%i): In Cool-Down Phase", _core->getId());
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

         // Send out packet
         LOG_PRINT("Sending Out Packet: Time(%llu)", _last_packet_send_time);
         _core->getNetwork()->netSend(net_packet, _last_packet_send_time);

         // Update Event Counters
         _total_packets_sent ++;
         _total_flits_sent += _network_model->computeSerializationLatency(&net_packet);

         break;
      }
   }
}

void SyntheticCore::netRecv(const NetPacket& net_packet)
{
   LOG_PRINT("Synthetic Core(%i): netRecv(%llu)", _core->getId(), net_packet.time);

   logNetRecv(net_packet.time);

   // Sample Latency
   UInt64 packet_latency = net_packet.time - net_packet.start_time;

   if (_warmup_phase_enabled)
   {
      updateBatchWarmupPhase(net_packet.time, packet_latency);
   }
   
   else if (_measurement_phase_enabled)
   {
      LOG_ASSERT_ERROR(_last_packet_recv_time <= net_packet.time,
                       "Last Recv Packet Time(%llu), Curr Packet Time(%llu)",
                       _last_packet_recv_time, net_packet.time);
      _last_packet_recv_time = net_packet.time;
      
      // Batch means method for sampling
      updateBatchMeasurementPhase(net_packet.time, packet_latency);
      
      // Event Counters
      _total_packets_received ++;
      _total_flits_received += _network_model->computeSerializationLatency(&net_packet);
      _total_packet_latency += packet_latency;
   }
}

// Warmup Phase
void SyntheticCore::startWarmupPhase()
{
   _warmup_phase_enabled = true;
   _warmup_state = 0;

   _curr_warmup_batch_num = 0;
   _warmup_batch_list.push_back(Batch());
}

bool SyntheticCore::endWarmupPhase(UInt64 time)
{
   _warmup_phase_enabled = false;
   _warmup_batch_list.clear();

   _num_cores_with_warmup_phase_completed ++;
   return (_num_cores_with_warmup_phase_completed == _num_cores);
}

void SyntheticCore::updateBatchWarmupPhase(UInt64 time, UInt64 sample_latency)
{
   Batch& curr_batch = _warmup_batch_list[_curr_warmup_batch_num];
   curr_batch._total_latency += sample_latency;
   curr_batch._size ++;

   if (curr_batch._size == _warmup_batch_size)
   {
      log("\nCore(%i)", _core->getId());
      log("Warmup Phase - Batch Num(%i), Batch Size(%i)", _curr_warmup_batch_num, _warmup_batch_size);
      
      if (_curr_warmup_batch_num > 0)
      {
         bool stationary = checkIfStationary();
         if (stationary)
         {
            log("Ending Warmup Phase");
            bool global_end_warmup_phase = endWarmupPhase(time);
            if (global_end_warmup_phase)
            {
               log("Globally Ending Warmup Phase");
               logimp("End Warm-Up Phase, Start Measurement Phase");
               for (SInt32 i = 0; i < _num_cores; i++)
               {
                  log("Core(%i): Start Measurement Phase[Time(%llu)]", _core->getId(), time);
                  SyntheticCore* synthetic_core = _synthetic_core_list[i];
                  synthetic_core->startMeasurementPhase(time);
               }
            }
         }
         else // Not Stationary
         {
            log("Warmup Phase continues: pushing another batch");
            _warmup_batch_list.push_back(Batch());
            _curr_warmup_batch_num ++;
         }
      }

      else // First Batch of Numbers
      {
         log("First Batch of Numbers: moving to next batch");
         _warmup_batch_list.push_back(Batch());
         _curr_warmup_batch_num ++;
      }

      log("\n");
   }
}

// Measurement Phase
void SyntheticCore::startMeasurementPhase(UInt64 time)
{
   _measurement_phase_enabled = true;
   _measurement_phase_start_time = time;

   _curr_measurement_batch_num = 0;
   _curr_measurement_batch_size = _batch_size_increment;
   _measurement_batch_list.resize(_num_measurement_batches, Batch());
}

bool SyntheticCore::endMeasurementPhase(UInt64 time)
{
   _measurement_phase_enabled = false;
   _measurement_phase_end_time = time;
   _measurement_batch_list.clear();

   _num_cores_with_measurement_phase_completed ++;
   return (_num_cores_with_measurement_phase_completed == _num_cores);
}

void SyntheticCore::updateBatchMeasurementPhase(UInt64 time, UInt64 sample_latency)
{
   Batch& curr_batch = _measurement_batch_list[_curr_measurement_batch_num];
   curr_batch._total_latency += sample_latency;
   curr_batch._size ++;

   // Update Curr Measurement Batch Num
   if (curr_batch._size == _curr_measurement_batch_size)
   {
      log("Core(%i): Finished measuring batch(%llu), moving onto(%llu)",
            _core->getId(), _curr_measurement_batch_num, _curr_measurement_batch_num+1);
      _curr_measurement_batch_num ++;
   }

   // Check Confidence if samples corresponding to all batches have been received
   if (_curr_measurement_batch_num == _num_measurement_batches)
   {
      log("\nCore(%i)", _core->getId());
      log("Checking confidence intervals: batch size(%llu)", _curr_measurement_batch_size);
      bool finished = checkConfidenceIntervals();
      if (finished)
      {
         log("End meaurement phase [Time(%llu)]", time);
         bool global_end_measurement_phase = endMeasurementPhase(time);
         if (global_end_measurement_phase)
         {
            log("Globally end measurement phase [Time(%llu)]", time);
            logimp("End Measurement Phase - Start Cool-Down Phase");
            globalStartCooldownPhase();
         }
      }
      else
      {
         log("not converged yet, getting next set of batches");
         _curr_measurement_batch_num = 0;
         _curr_measurement_batch_size += _batch_size_increment;
      }
   }
}

// Cooldown Phase
void SyntheticCore::globalStartCooldownPhase()
{   
   for (SInt32 i = 0; i < _num_cores; i++)
   {
      log("Core(%i): start cooldown phase", _core->getId());
      SyntheticCore* synthetic_core = _synthetic_core_list[i];
      synthetic_core->startCooldownPhase();
   }
}

void SyntheticCore::startCooldownPhase()
{
   _cooldown_phase_enabled = true;
}

void SyntheticCore::endCooldownPhase()
{
   _cooldown_phase_enabled = false;
}

// Utilities
bool SyntheticCore::checkIfStationary()
{
   log("checkIfStationary() start");
   Batch& curr_batch = _warmup_batch_list[_curr_warmup_batch_num];
   double avg_latency_curr_batch = ((double) curr_batch._total_latency) / _warmup_batch_size;

   // Start cooldown phase if average latency is too large
   if (avg_latency_curr_batch > 5000)
   {
      logimp("Latency Too Large !!  End Warm-Up Phase - Start Cool-Down Phase");
      globalStartCooldownPhase();
   }
  
   if (_warmup_state == 0)
   {
      Batch& prev_batch = _warmup_batch_list[_curr_warmup_batch_num-1];
      double avg_latency_prev_batch = ((double) prev_batch._total_latency) / _warmup_batch_size;
      _ref_avg_latency = avg_latency_prev_batch;
   }

   log("Curr Batch Latency(%g), Ref Batch Latency(%g)", avg_latency_curr_batch, _ref_avg_latency);

   double fractional_difference = abs(avg_latency_curr_batch - _ref_avg_latency) / _ref_avg_latency;
   log("Fractional Difference(%g)", fractional_difference);
   
   if (_curr_warmup_batch_num > 20)
   {
      logimp("Too Much Time To Warm-Up. Fast-Forward To Measurement Phase");
      return true;
   }

   // Go-back to initial state if fractional difference is too large
   if (fractional_difference > 0.2)
   {
      _warmup_state = 0;
      return false;
   }

   if (_warmup_state == 0)
   {
      if (avg_latency_curr_batch < _ref_avg_latency)
         _warmup_state = 1;
   }
   else if (_warmup_state >= 1 && _warmup_state < 4)
   {
      _warmup_state ++;
   }
   else
   {
      LOG_PRINT_ERROR("Unrecognized State(%i)", _warmup_state);
   }
   
   log("Warmup state(%i)", _warmup_state);
   return (_warmup_state == 4) ? true : false;
}

bool SyntheticCore::checkConfidenceIntervals()
{
   log("checkConfidenceIntervals() start");
   double batch_mean = computeBatchMean();
   double batch_stddev = computeBatchStdDev(batch_mean);
   // Average Computed based on Dally et al. (Computer Networks book)
   // 99% Confidence Interval
   double t_distribution_val = 2.4;
   log("Mean(%g), Stddev(%g)", batch_mean, batch_stddev);
   UInt64 num_samples = _num_measurement_batches * _curr_measurement_batch_size;
   double error = 2 * batch_stddev * t_distribution_val / sqrt(num_samples);
   log("error(%g), error/batch_mean(%g)", error, error/batch_mean);
   if ((error / batch_mean) < 0.01)
      return true;
   else
      return false;
}

double SyntheticCore::computeBatchMean()
{
   UInt64 total_latency = 0;
   UInt64 num_samples = _num_measurement_batches * _curr_measurement_batch_size;
   for (SInt32 i = 0; i < (SInt32) _num_measurement_batches; i++)
      total_latency += _measurement_batch_list[i]._total_latency;
   return ((double) total_latency) / num_samples;
}

double SyntheticCore::computeBatchStdDev(double batch_mean)
{
   double sum = 0.0;
   for (SInt32 i = 0; i < (SInt32) _num_measurement_batches; i++)
   {
      Batch& curr_batch = _measurement_batch_list[i];
      double batch_latency = ((double) curr_batch._total_latency) / curr_batch._size;
      assert(curr_batch._size == _curr_measurement_batch_size);
      sum += pow(batch_latency - batch_mean, 2);
   }
   return sqrt(sum / _num_measurement_batches);
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
   if (_core->getId() == 0)
   {
      // log("Core(0) sending packet[Time(%llu)]", (long long unsigned int) time);
   }
}

void SyntheticCore::logNetRecv(UInt64 time)
{
   if (_core->getId() == 0)
   {
      // log("Core(0) receiving packet[Time(%llu)]", (long long unsigned int) time);
   }
}

void SyntheticCore::outputSummary(ostream& out)
{
   out << "Synthetic Core Summary: " << endl;
   
   // Send
   double offered_broadcast_load = _fraction_broadcasts * _offered_load * _broadcast_packet_size;
   double offered_unicast_load = _offered_load * (1 - _fraction_broadcasts) * _unicast_packet_size;
   out << "    Offered Unicast Load (in flits/clock-cycle): " << offered_unicast_load << endl;
   out << "    Offered Broadcast Load (in flits/clock-cycle): " << offered_broadcast_load << endl;
   out << "    Total Offered Load (in flits/clock-cycle): " << offered_unicast_load + offered_broadcast_load << endl;

   // Receive
   out << "    Total Packets Received: " << _total_packets_received << endl;
   out << "    Total Flits Received: " << _total_flits_received << endl;
   UInt64 measurement_phase_time = _measurement_phase_end_time - _measurement_phase_start_time;
   double sustained_throughput = ((double) _total_flits_received) / measurement_phase_time;
   double average_packet_latency = ((double) _total_packet_latency) / _total_packets_received;
   out << "    Measurement Phase Time (in clock-cycles): " << measurement_phase_time << endl;
   out << "    Measurement Phase Start Time (in clock-cycles): " << _measurement_phase_start_time << endl;
   out << "    Measurement Phase End Time (in clock-cycles): " << _measurement_phase_end_time << endl;
   out << "    Average Latency (in clock-cycles): " << average_packet_latency << endl;
   out << "    Sustained Throughput (in flits/clock-cycle): " << sustained_throughput << endl;
}
