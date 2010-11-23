#ifndef NETWORK_H
#define NETWORK_H

#include <iostream>
#include <vector>
#include <list>
using namespace std;

#include "packet_type.h"
#include "fixed_types.h"
#include "cond.h"
#include "transport.h"
#include "network_model.h"

// TODO: Do we need to support multicast to some (but not all)
// destinations?

class Core;
class Network;

// -- Network Packets -- //

class NetPacket
{
public:
   UInt64 start_time;
   UInt64 time;
   PacketType type;
   
   SInt32 sender;
   SInt32 receiver;

   UInt32 length;
   const void *data;

   // Is Raw/Modeling Packet
   bool is_raw;
   // Sequence Number
   UInt64 sequence_num;

   // This field may be used by specific network models in whatever way they please
   UInt32 specific;
   
   // Constructors
   NetPacket();
   explicit NetPacket(Byte*);
   NetPacket(UInt64 time, PacketType type, UInt32 length, const void *data, \
             bool is_raw = true, UInt64 sequence_num = 0);
   NetPacket(UInt64 time, PacketType type, SInt32 sender, \
             SInt32 receiver, UInt32 length, const void *data, \
             bool is_raw = true, UInt64 sequence_num = 0);

   UInt32 bufferSize() const;
   Byte* makeBuffer() const;
   NetPacket* clone() const;
   void release();

   static const SInt32 BROADCAST = 0xDEADBABE;
};

typedef list<NetPacket> NetQueue;

// -- Network Matches -- //

class NetMatch
{
   public:
      vector<SInt32> senders;
      vector<PacketType> types;
};

// -- Network -- //

// This is the managing class that interacts with the physical
// transport layer to forward packets from source to destination.

class Network
{
   public:
      // -- Ctor, housekeeping, etc. -- //
      Network(Core *core);
      ~Network();

      Core *getCore() const { return _core; }
      Transport::Node *getTransport() const { return _transport; }

      typedef void (*NetworkCallback)(void*, NetPacket);

      void registerCallback(PacketType type,
                            NetworkCallback callback,
                            void *obj);

      void unregisterCallback(PacketType type);

      void outputSummary(ostream &out) const;

      void netPullFromTransport();

      // -- Main interface -- //

      SInt32 netSend(NetPacket& packet);
      NetPacket netRecv(const NetMatch &match);

      // -- Wrappers -- //

      SInt32 netSend(SInt32 dest, PacketType type, const void *buf, UInt32 len);
      SInt32 netBroadcast(PacketType type, const void *buf, UInt32 len);
      NetPacket netRecv(SInt32 src, PacketType type);
      NetPacket netRecvFrom(SInt32 src);
      NetPacket netRecvType(PacketType type);

      void enableModels();
      void disableModels();
      void resetModels();

      // -- Network Models -- //
      NetworkModel* getNetworkModelFromPacketType(PacketType packet_type);

      // Modeling
      UInt32 getModeledLength(const NetPacket& pkt);

   private:
      NetworkModel * _models[NUM_STATIC_NETWORKS];

      NetworkCallback *_callbacks;
      void **_callbackObjs;

      Core *_core;
      Transport::Node *_transport;

      SInt32 _tid;
      SInt32 _numMod;

      NetQueue _netQueue;
      Lock _netQueueLock;
      ConditionVariable _netQueueCond;

      SInt32 forwardPacket(const NetPacket& packet);
      void sendPacket(const NetPacket* packet);
      void sendPacketList(const list<NetPacket*>& net_packet_list_to_send);
      void receivePacket(NetPacket& packet);
      void receivePacketList(const list<NetPacket*>& net_packet_list_to_receive);
};

#endif // NETWORK_H
