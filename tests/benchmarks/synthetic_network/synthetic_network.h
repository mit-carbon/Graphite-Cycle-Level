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
   Core* _core;
   FiniteBufferNetworkModel* _network_model;
   RandNum* _inject_packet_rand_num;
   RandNum* _broadcast_packet_rand_num;
   vector<int> _send_vec;
   vector<int> _receive_vec;
   UInt64 _total_packets_sent;
   UInt64 _total_flits_sent;
   UInt64 _total_packets_received;
   UInt64 _total_flits_received;
   UInt64 _last_packet_send_time;
   UInt64 _last_packet_recv_time;
   
   bool canInjectPacket();
   bool isBroadcastPacket();
   void logNetSend(UInt64 time);
   void logNetRecv(UInt64 time);
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

void debug_printf(const char* fmt, ...);
