#pragma once

#include <stdlib.h>
#include <stdarg.h>
#include <vector>
#include <string>
using std::vector;
using std::string;

#include "event.h"
#include "rand_num.h"

class SyntheticCore
{
public:
   SyntheticCore(Core* core, const vector<int>& send_vec, const vector<int>& receive_vec);
   ~SyntheticCore();

   void processNetSendEvent(Event* event);
   void processNetRecvEvent(const NetPacket& net_packet);
   void pushNetSendEvent(UInt64 time);

private:
   Core* _core;
   RandNum* _inject_packet_rand_num;
   RandNum* _broadcast_packet_rand_num;
   vector<int> _send_vec;
   vector<int> _receive_vec;
   UInt64 _total_packets_sent;
   UInt64 _total_packets_received;
   UInt64 _last_packet_time;
   
   bool canInjectPacket();
   bool isBroadcastPacket();
   void logNetSend(UInt64 time);
   void logNetRecv(UInt64 time);
   static SInt32 computeNumFlits(SInt32 length);

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
void processNetRecvEvent(void* obj, NetPacket net_packet);

void registerNetRecvHandler();
void unregisterNetRecvHandler();

void waitForCompletion();

void printHelpMessage();

void debug_printf(const char* fmt, ...);
