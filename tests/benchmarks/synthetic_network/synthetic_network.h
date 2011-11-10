#pragma once

#include <stdlib.h>
#include <stdarg.h>
#include <vector>
#include <string>
using std::vector;
using std::string;

#include "finite_buffer_network_model.h"
#include "event.h"
#include "rand_num.h"

class SyntheticCore
{
public:
   SyntheticCore(Core* core, const vector<int>& send_vec, const vector<int>& receive_vec);
   ~SyntheticCore();

   Core* getCore() { return _core; }
   void netSend(UInt64 time);
   void netRecv(const NetPacket& net_packet);
   void outputSummary(ostream& out);

private:
   class Batch
   {
   public:
      Batch(): _total_latency(0), _size(0) {}
      ~Batch() {}
      UInt64 _total_latency;
      UInt64 _size;
   };
      
   Core* _core;
   FiniteBufferNetworkModel* _network_model;
   RandNum* _inject_packet_rand_num;
   RandNum* _broadcast_packet_rand_num;
   vector<int> _send_vec;
   vector<int> _receive_vec;
   
   // Event Counters
   UInt64 _total_packets_sent;
   UInt64 _total_flits_sent;
   UInt64 _total_packets_received;
   UInt64 _total_flits_received;
   UInt64 _total_packet_latency;
   // For error-checking
   UInt64 _last_packet_send_time;
   UInt64 _last_packet_recv_time;

   // -- Warmup Phase
   bool _warmup_phase_enabled;
   SInt32 _warmup_state;
   static const UInt64 _warmup_batch_size = 1000;
   // Book-keeping variables
   double _ref_avg_latency;
   vector<Batch> _warmup_batch_list;
   UInt64 _curr_warmup_batch_num;

   // -- Measurement Phase
   bool _measurement_phase_enabled;
   static const UInt64 _num_measurement_batches = 30;
   static const UInt64 _batch_size_increment = 1000;
   UInt64 _curr_measurement_batch_size;
   // Book-keeping variables 
   UInt64 _curr_measurement_batch_num;
   vector<Batch> _measurement_batch_list;
   // Start & End Times
   UInt64 _measurement_phase_start_time;
   UInt64 _measurement_phase_end_time;

   // -- Cooldown Phase
   bool _cooldown_phase_enabled;
   
   bool canInjectPacket();
   bool isBroadcastPacket();
   void logNetSend(UInt64 time);
   void logNetRecv(UInt64 time);

   // Update Batch Totals and Means
   void updateBatchWarmupPhase(UInt64 time, UInt64 sample_latency);
   void updateBatchMeasurementPhase(UInt64 time, UInt64 sample_latency);

   // Different Phases
   void startWarmupPhase();
   bool endWarmupPhase(UInt64 time);
   void startMeasurementPhase(UInt64 time);
   bool endMeasurementPhase(UInt64 time);
   void globalStartCooldownPhase();
   void startCooldownPhase();
   void endCooldownPhase();

   // Utilites
   bool checkIfStationary();
   bool checkConfidenceIntervals();
   double computeBatchMean();
   double computeBatchStdDev(double batch_mean);
};

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

void initializeSyntheticCores();
void deinitializeSyntheticCores();

void processStartSimulationEvent(Event* event);
void processNetSendEvent(Event* event);
void netPacketInjectorExitCallback(void* obj, UInt64 time);
void asyncNetRecvCallback(void* obj, NetPacket net_packet);

void registerNetPacketInjectorExitHandler();
void registerAsyncNetRecvHandler();
void unregisterNetPacketInjectorExitHandler();
void unregisterAsyncNetRecvHandler();

void waitForCompletion();

void outputSummary(void* callback_obj, ostream& out);

void printHelpMessage();

void logimp(const char* fmt, ...);
void log(const char* fmt, ...);
