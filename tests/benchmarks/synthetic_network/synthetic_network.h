#pragma once

#include <stdlib.h>
#include <stdarg.h>
#include <vector>
#include <string>
using std::vector;
using std::string;

#include "event.h"

class RandNum
{
public:
   RandNum(double start, double end, SInt32 seed):
      _start(start), _end(end)
   { 
      srand48_r(seed, &rand_buffer); 
   }
   ~RandNum() {}
   double next()
   {
      double result;
      drand48_r(&rand_buffer, &result);
      return (result * (_end - _start) + _start);
   }

private:
   struct drand48_data rand_buffer;
   double _start;
   double _end;
};

class CoreSpVars
{
public:
   CoreSpVars() : _total_packets_sent(0), _total_packets_received(0), _last_packet_time(0) {}
   ~CoreSpVars() {}
   void init(RandNum* rand_num, vector<int>& send_vec, vector<int>& receive_vec)
   {
      _rand_num = rand_num;
      _send_vec = send_vec;
      _receive_vec = receive_vec;
   }

   RandNum* _rand_num;
   UInt64 _total_packets_sent;
   UInt64 _total_packets_received;
   vector<int> _send_vec;
   vector<int> _receive_vec;
   UInt64 _last_packet_time;
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

void printHelpMessage();

void registerRecvdPacketHandler();
void unregisterRecvdPacketHandler();

void waitForCompletion();
void initializeCoreSpVars();
void deinitializeCoreSpVars();

void processNetSendEvent(Event* event);
void processRecvdPacket(void* obj, NetPacket net_packet);
void pushFirstEvents(Event* event);
void pushEvent(UInt64 time, Core* core);

bool canSendPacket(double offered_load, RandNum* rand_num);
SInt32 computeNumFlits(SInt32 length);

void debug_printf(const char* fmt, ...);
