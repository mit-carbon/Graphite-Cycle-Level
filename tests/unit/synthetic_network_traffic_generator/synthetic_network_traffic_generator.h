#pragma once

#include <stdlib.h>
#include <vector>
#include <string>
using std::vector;
using std::string;

#include "semaphore.h"
#include "event.h"

class RandNum
{
public:
   RandNum(double start, double end):
      _start(start), _end(end) 
   { 
      srand48_r(time(NULL), &rand_buffer); 
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
   CoreSpVars() : _total_packets_sent(0), _last_packet_time(0) {}
   ~CoreSpVars() {}
   void init(RandNum* rand_num, vector<int>& send_vec, Semaphore* send_semaphore)
   {
      _rand_num = rand_num;
      _send_vec = send_vec;
      _send_semaphore = send_semaphore;
   }

   RandNum* _rand_num;
   UInt64 _total_packets_sent;
   vector<int> _send_vec;
   Semaphore* _send_semaphore;
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

void* sendNetworkTraffic(void*);
void uniformRandomTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);
void bitComplementTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);
void shuffleTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);
void transposeTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);
void tornadoTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);
void nearestNeighborTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);

bool canSendPacket(double offered_load, RandNum* rand_num);
void synchronize(UInt64 time, Core* core);
void printHelpMessage();
NetworkTrafficType parseTrafficPattern(string traffic_pattern);

void processNetSendEvent(Event* event);
void pushEvent(UInt64 time, Core* core);
void pushFirstEvents(Event* event);

SInt32 computeNumFlits(SInt32 length);
