#ifndef NETWORK_H
#define NETWORK_H

#include <iostream>
#include <vector>
#include <list>
using namespace std;

#include "packet_type.h"
#include "fixed_types.h"
#include "cond.h"
#include "network_model.h"

class Core;
class Network;

// -- Network Packets -- //

class NetPacket
{
public:
   UInt64 start_time;
   UInt64 time;
   PacketType type;
   
   core_id_t sender;
   core_id_t receiver;

   UInt32 length;
   const void *data;

   // Is Raw/Modeling Packet
   bool is_raw;
   // Sequence Number
   UInt32 sequence_num;

   // This field may be used by specific network models in whatever way they please
   UInt32 specific;
   
   // Constructors
   NetPacket();
   explicit NetPacket(Byte*);
   NetPacket(UInt64 time, PacketType type,
             UInt32 length, const void *data,
             bool is_raw = true, UInt32 sequence_num = 0);
   NetPacket(UInt64 time, PacketType type,
             core_id_t sender, core_id_t receiver,
             UInt32 length, const void *data,
             bool is_raw = true, UInt32 sequence_num = 0);

   UInt32 bufferSize() const;
   Byte* makeBuffer() const;
   NetPacket* clone() const;
   void release();

   static const SInt32 BROADCAST = 0xDEADBABE;
};

typedef list<NetPacket*> NetQueue;

// -- Network Matches -- //

class NetMatch
{
public:
   NetMatch();
   NetMatch(core_id_t sender, PacketType pkt_type);
   NetMatch(core_id_t sender);
   NetMatch(PacketType pkt_type);
   NetMatch(const vector<core_id_t>& senders_, const vector<PacketType>& types_);

   vector<core_id_t> senders;
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

   typedef void (*NetRecvCallback)(void*, NetPacket);

   // Register and Unregister Callbacks
   void registerAsyncRecvCallback(PacketType type,
                                  NetRecvCallback callback,
                                  void* obj);
   void unregisterAsyncRecvCallback(PacketType type);

   void outputSummary(ostream &out) const;

   void processPacket(NetPacket* packet);

   // -- Main interface -- //

   SInt32 netSend(NetPacket& packet, UInt64 start_time = UINT64_MAX_);
   void netRecv(const NetMatch &match, NetRecvCallback callback, void* callbackObj);

   // -- Wrappers -- //

   SInt32 netSend(SInt32 dest, PacketType type, const void *buf, UInt32 len);
   SInt32 netBroadcast(PacketType type, const void *buf, UInt32 len);

   // -- Enable/Disable Models -- //

   void enableModels();
   void disableModels();

   // -- Network Models -- //

   NetworkModel* getNetworkModelFromPacketType(PacketType packet_type);
   PacketType getPacketTypeFromNetworkId(SInt32 network_id);

   // -- Utilities -- //
   
   UInt32 getModeledLength(const NetPacket& pkt);
   bool isModeled(const NetPacket& packet);
   core_id_t getRequester(const NetPacket& packet);

private:
   NetworkModel * _models[NUM_STATIC_NETWORKS];

   // For Asynchronous Recvs'
   NetRecvCallback *_asyncRecvCallbacks;
   void **_asyncRecvCallbackObjs;
   
   // For Synchronous Recvs'
   NetRecvCallback _syncRecvCallback;
   void* _syncRecvCallbackObj;
   NetMatch _syncRecvMatch;

   Core *_core;
   SInt32 _numMod;

   NetQueue _netQueue;

   bool _enabled;

   // Processing Packets
   SInt32 forwardPacket(const NetPacket* packet);
   void sendPacket(const NetPacket* packet, SInt32 receiver);
   void sendPacketList(const list<NetPacket*>& net_packet_list_to_send);
   void receivePacket(NetPacket* packet);
   void receivePacketList(const list<NetPacket*>& net_packet_list_to_receive);

   // Sync NetRecv
   void processSyncRecv();
};

#endif // NETWORK_H
