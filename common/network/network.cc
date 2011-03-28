#define __STDC_LIMIT_MACROS
#include <climits>
#include <algorithm>
#include <string.h>

#include "transport.h"
#include "core.h"
#include "network.h"
#include "memory_manager.h"
#include "simulator.h"
#include "core_manager.h"
#include "clock_converter.h"
#include "fxsupport.h"
#include "finite_buffer_network_model.h"
#include "event.h"
#include "log.h"

using namespace std;

Network::Network(Core *core)
      : _core(core)
      , _enabled(false)
{
   LOG_ASSERT_ERROR(sizeof(g_type_to_static_network_map) / sizeof(EStaticNetwork) == NUM_PACKET_TYPES,
                    "Static network type map has incorrect number of entries.");

   _numMod = Config::getSingleton()->getTotalCores();
   _tid = _core->getId();

   _callbacks = new NetworkCallback [NUM_PACKET_TYPES];
   _callbackObjs = new void* [NUM_PACKET_TYPES];
   for (SInt32 i = 0; i < NUM_PACKET_TYPES; i++)
      _callbacks[i] = NULL;

   for (SInt32 i = 0; i < NUM_STATIC_NETWORKS; i++)
   {
      UInt32 network_model = NetworkModel::parseNetworkType(Config::getSingleton()->getNetworkType(i));
      
      _models[i] = NetworkModel::createModel(this, i, network_model);
   }

   LOG_PRINT("Initialized.");
}

Network::~Network()
{
   for (SInt32 i = 0; i < NUM_STATIC_NETWORKS; i++)
      delete _models[i];

   delete [] _callbackObjs;
   delete [] _callbacks;

   LOG_PRINT("Destroyed.");
}

void Network::registerCallback(PacketType type, NetworkCallback callback, void *obj)
{
   assert((UInt32)type < NUM_PACKET_TYPES);
   _callbacks[type] = callback;
   _callbackObjs[type] = obj;
}

void Network::unregisterCallback(PacketType type)
{
   assert((UInt32)type < NUM_PACKET_TYPES);
   _callbacks[type] = NULL;
}

void Network::outputSummary(std::ostream &out) const
{
   out << "Network summary:\n";
   for (UInt32 i = 0; i < NUM_STATIC_NETWORKS; i++)
   {
      out << "  Network model " << i << ":\n";
      _models[i]->outputSummary(out);
   }
}

core_id_t Network::getRequester(const NetPacket& packet)
{
   // USER network -- (packet.sender)
   // SHARED_MEM network -- (getRequester(packet))
   // SYSTEM network -- INVALID_CORE_ID
   SInt32 network_id = (getNetworkModelFromPacketType(packet.type))->getNetworkId();
   if ((network_id == STATIC_NETWORK_USER_1) || (network_id == STATIC_NETWORK_USER_2))
      return packet.sender;
   else if ((network_id == STATIC_NETWORK_MEMORY_1) || (network_id == STATIC_NETWORK_MEMORY_2))
      return getCore()->getMemoryManager()->getShmemRequester(packet.data);
   else // (network_id == STATIC_NETWORK_SYSTEM)
      return packet.sender; 
}

bool Network::isModeled(const NetPacket& packet)
{
   // USER network -- (true)
   // SHARED_MEM network -- (requester < Config::getSingleton()->getApplicationCores())
   // SYSTEM network -- (false)
   core_id_t requester = getRequester(packet);
   SInt32 network_id = (getNetworkModelFromPacketType(packet.type))->getNetworkId();
   if ((network_id == STATIC_NETWORK_USER_1) || (network_id == STATIC_NETWORK_USER_2))
      return true;
   else if ((network_id == STATIC_NETWORK_MEMORY_1) || (network_id == STATIC_NETWORK_MEMORY_2))
      return (requester < (core_id_t) Config::getSingleton()->getApplicationCores());
   else // (network_id == STATIC_NETWORK_SYSTEM)
      return false;
}

// Function that receives packets from the event queue
void Network::processPacket(NetPacket* packet)
{
   LOG_PRINT("Got packet : type %i, from %i, time %llu, raw %i", \
         (SInt32) packet->type, packet->sender, packet->time, packet->is_raw);
   LOG_ASSERT_ERROR(0 <= packet->sender && packet->sender < _numMod,
         "Invalid Packet Sender(%i)", packet->sender);
   LOG_ASSERT_ERROR(0 <= packet->type && packet->type < NUM_PACKET_TYPES,
         "Packet type: %d not between 0 and %d", packet->type, NUM_PACKET_TYPES);

   NetworkModel* model = getNetworkModelFromPacketType(packet->type);

   if (model->isFiniteBuffer())
   {
      FiniteBufferNetworkModel* finite_buffer_model = (FiniteBufferNetworkModel*) model;

      list<NetPacket*> net_packet_list_to_send;
      list<NetPacket*> net_packet_list_to_receive;
      finite_buffer_model->receiveNetPacket(packet, net_packet_list_to_send, net_packet_list_to_receive);
      
      // Free the memory occupied by the packet (header + data)
      packet->release();

      // Send packets destined for other cores
      sendPacketList(net_packet_list_to_send);
      
      // Receive the packets intended for this core
      receivePacketList(net_packet_list_to_receive);
   }
   else // (!model->isFiniteBuffer())
   {
      UInt32 action = model->computeAction(*packet);
      
      if (action & NetworkModel::RoutingAction::FORWARD)
      {
         LOG_PRINT("Forwarding packet : type %i, from %i, to %i, core_id %i, time %llu.", 
               (SInt32)packet->type, packet->sender, packet->receiver, _core->getId(), packet->time);
         forwardPacket(packet);
      }
      if (action & NetworkModel::RoutingAction::RECEIVE)
      {
         receivePacket(packet);
      }
      else // if (!(action & NetworkModel::RoutingAction::RECEIVE))
      {
         packet->release();
      }
   }
}

void Network::receivePacket(NetPacket* packet)
{
   LOG_PRINT("receivePacket(%p) enter", packet);

   NetworkModel* model = getNetworkModelFromPacketType(packet->type);
   // I have accepted the packet - process the received packet
   model->processReceivedPacket(*packet);

   LOG_PRINT("Before Converting Cycle Count: packet->time(%llu)", packet->time);
   
   // Convert time (cycle count) from network frequency to core frequency
   packet->time = convertCycleCount(packet->time,
         getNetworkModelFromPacketType(packet->type)->getFrequency(),
         _core->getPerformanceModel()->getFrequency());

   LOG_PRINT("After Converting Cycle Count: packet->time(%llu)", packet->time);

   // asynchronous I/O support
   NetworkCallback callback = _callbacks[packet->type];

   if (callback != NULL)
   {
      LOG_PRINT("Executing callback on packet : type %i, from %i, to %i, core_id %i, cycle_count %llu",
            (SInt32) packet->type, packet->sender, packet->receiver, _core->getId(), packet->time);
      assert(0 <= packet->sender && packet->sender < _numMod);
      assert(0 <= packet->type && packet->type < NUM_PACKET_TYPES);

      callback(_callbackObjs[packet->type], *packet);

      packet->release();
   }

   // synchronous I/O support
   else
   {
      // Signal the app thread that a packet is available
      LOG_PRINT("Enqueuing packet : type %i, from %i, to %i, core_id %i, cycle_count %llu",
            (SInt32) packet->type, packet->sender, packet->receiver, _core->getId(), packet->time);
      _netQueueLock.acquire();
      _netQueue.push_back(packet);
      _netQueueLock.release();
      _netQueueCond.broadcast();
   }
   
   LOG_PRINT("receivePacket(%p) exit", packet);
}

void Network::receivePacketList(const list<NetPacket*>& net_packet_list_to_receive)
{
   LOG_PRINT("receivePacketList() enter");
   
   list<NetPacket*>::const_iterator it = net_packet_list_to_receive.begin();
   for ( ; it != net_packet_list_to_receive.end(); it ++)
   {
      NetPacket* packet_to_receive = *it;
      assert(packet_to_receive->is_raw);
      receivePacket(packet_to_receive);
   }
 
   LOG_PRINT("receivePacketList() exit");
}

void Network::sendPacket(const NetPacket* packet, SInt32 next_hop)
{
   LOG_PRINT("sendPacket(%p) enter", packet);
   LOG_PRINT("sendPacket(): time(%llu), type(%i), sender(%i), receiver(%i), network_name(%s)",
         packet->time, packet->type, packet->sender, next_hop,
         getNetworkModelFromPacketType(packet->type)->getNetworkName().c_str());

   // FIXME: Decide about the event_queue_type
   UnstructuredBuffer event_args;
   event_args << next_hop << packet;
   EventNetwork* event = new EventNetwork(packet->time, event_args);
   // EventQueue::Type event_queue_type = ((_enabled) && (isModeled(*packet))) ?
   //                                     EventQueue::ORDERED : EventQueue::UNORDERED;
   EventQueue::Type event_queue_type = (g_type_to_static_network_map[packet->type] == STATIC_NETWORK_USER_2) ?
                                       EventQueue::ORDERED : EventQueue::UNORDERED;
   Event::processInOrder(event, next_hop, event_queue_type);

   LOG_PRINT("sendPacket(%p) exit", packet);
}

void Network::sendPacketList(const list<NetPacket*>& net_packet_list_to_send)
{
   LOG_PRINT("sendPacketList() enter");
   
   // Send the network packets
   list<NetPacket*>::const_iterator it = net_packet_list_to_send.begin();
   for ( ; it != net_packet_list_to_send.end(); it ++)
   {
      NetPacket* packet_to_send = *it;
      sendPacket(packet_to_send, packet_to_send->receiver);
   }
   
   LOG_PRINT("sendPacketList() exit");
}

SInt32 Network::forwardPacket(const NetPacket* packet)
{
   LOG_ASSERT_ERROR((packet->type >= 0) && (packet->type < NUM_PACKET_TYPES),
         "packet->type(%u)", packet->type);

   NetworkModel *model = getNetworkModelFromPacketType(packet->type);

   vector<NetworkModel::Hop> hopVec;
   model->routePacket(*packet, hopVec);

   for (UInt32 i = 0; i < hopVec.size(); i++)
   {
      LOG_PRINT("Send packet : type %i, from %i, to %i, next_hop %i, core_id %i, time %llu",
            (SInt32) packet->type, packet->sender, hopVec[i].final_dest, hopVec[i].next_dest,
            _core->getId(), hopVec[i].time);

      NetPacket* cloned_packet = packet->clone();
      cloned_packet->time = hopVec[i].time;
      cloned_packet->receiver = hopVec[i].final_dest;
      cloned_packet->specific = hopVec[i].specific;

      sendPacket(cloned_packet, hopVec[i].next_dest);
       
      LOG_PRINT("Sent packet");
   }

   return packet->length;
}

NetworkModel* Network::getNetworkModelFromPacketType(PacketType packet_type)
{
   return _models[g_type_to_static_network_map[packet_type]];
}

SInt32 Network::netSend(NetPacket& packet)
{
   // Interface for sending packets on a network

   // Floating Point Save/Restore
   FloatingPointHandler floating_point_handler;

   // Convert time from core frequency to network frequency
   packet.time = convertCycleCount(packet.time, \
         _core->getPerformanceModel()->getFrequency(), \
         getNetworkModelFromPacketType(packet.type)->getFrequency());

   // Note the start time
   packet.start_time = packet.time;

   NetPacket* cloned_packet = packet.clone();

   NetworkModel* model = getNetworkModelFromPacketType(cloned_packet->type);
   if (model->isFiniteBuffer())
   {
      // Call FiniteBufferNetworkModel functions
      FiniteBufferNetworkModel* finite_buffer_model = (FiniteBufferNetworkModel*) model;
      
      // Divide Packet into flits
      list<NetPacket*> net_packet_list_to_send;
      finite_buffer_model->sendNetPacket(cloned_packet, net_packet_list_to_send);
      
      // FIXME: Verify this works for broadcasted packets
      // Send Raw Packet on the network
      sendPacket(cloned_packet, cloned_packet->receiver);
      // Send Flits on the network (also free the memory occupied by flits)
      sendPacketList(net_packet_list_to_send);

      return cloned_packet->length;
   }
   else // (!model->isFiniteBuffer())
   {
      // Call forwardPacket(cloned_packet)
      return forwardPacket(cloned_packet);
   }
}

// Stupid helper class to eliminate special cases for empty
// sender/type vectors in a NetMatch
class NetRecvIterator
{
   public:
      NetRecvIterator(UInt32 i)
            : _mode(INT)
            , _max(i)
            , _i(0)
      {
      }
      NetRecvIterator(const std::vector<SInt32> &v)
            : _mode(SENDER_VECTOR)
            , _senders(&v)
            , _i(0)
      {
      }
      NetRecvIterator(const std::vector<PacketType> &v)
            : _mode(TYPE_VECTOR)
            , _types(&v)
            , _i(0)
      {
      }

      inline UInt32 get()
      {
         switch (_mode)
         {
         case INT:
            return _i;
         case SENDER_VECTOR:
            return (UInt32)_senders->at(_i);
         case TYPE_VECTOR:
            return (UInt32)_types->at(_i);
         default:
            assert(false);
            return (UInt32)-1;
         };
      }

      inline Boolean done()
      {
         switch (_mode)
         {
         case INT:
            return _i >= _max;
         case SENDER_VECTOR:
            return _i >= _senders->size();
         case TYPE_VECTOR:
            return _i >= _types->size();
         default:
            assert(false);
            return true;
         };
      }

      inline void next()
      {
         ++_i;
      }

      inline void reset()
      {
         _i = 0;
      }

   private:
      enum
      {
         INT, SENDER_VECTOR, TYPE_VECTOR
      } _mode;

      union
      {
         UInt32 _max;
         const std::vector<SInt32> *_senders;
         const std::vector<PacketType> *_types;
      };

      UInt32 _i;
};

NetPacket* Network::netRecv(const NetMatch &match)
{
   LOG_PRINT("Entering netRecv.");

   // Track via iterator to minimize copying
   NetQueue::iterator itr;
   Boolean found;

   found = false;

   NetRecvIterator sender = match.senders.empty()
                            ? NetRecvIterator(_numMod)
                            : NetRecvIterator(match.senders);

   NetRecvIterator type = match.types.empty()
                          ? NetRecvIterator((UInt32)NUM_PACKET_TYPES)
                          : NetRecvIterator(match.types);

   LOG_ASSERT_ERROR(_core && _core->getPerformanceModel(),
                    "Core and/or performance model not initialized.");
   UInt64 start_time = _core->getPerformanceModel()->getCycleCount();

   _netQueueLock.acquire();

   while (!found)
   {
      itr = _netQueue.end();

      // check every entry in the queue
      for (NetQueue::iterator i = _netQueue.begin();
            i != _netQueue.end();
            i++)
      {
         // only find packets that match
         for (sender.reset(); !sender.done(); sender.next())
         {
            if ((*i)->sender != (SInt32)sender.get())
               continue;

            for (type.reset(); !type.done(); type.next())
            {
               if ((*i)->type != (PacketType)type.get())
                  continue;

               found = true;

               // find the earliest packet
               if (itr == _netQueue.end() ||
                   (*itr)->time > (*i)->time)
               {
                  itr = i;
               }
            }
         }
      }

      // go to sleep until a packet arrives if none have been found
      if (!found)
      {
         _netQueueCond.wait(_netQueueLock);
      }
   }

   assert(found == true && itr != _netQueue.end());
   assert(0 <= (*itr)->sender && (*itr)->sender < _numMod);
   assert(0 <= (*itr)->type && (*itr)->type < NUM_PACKET_TYPES);
   assert(((*itr)->receiver == _core->getId()) || ((*itr)->receiver == NetPacket::BROADCAST));

   // Copy result
   NetPacket* packet = (*itr);
   _netQueue.erase(itr);
   _netQueueLock.release();

   LOG_PRINT("packet.time(%llu), start_time(%llu)", packet->time, start_time);

   if (packet->time > start_time)
   {
      LOG_PRINT("Queueing RecvInstruction(%llu)", packet->time - start_time);
      Instruction *i = new RecvInstruction(packet->time - start_time);
      _core->getPerformanceModel()->queueDynamicInstruction(i);
   }

   LOG_PRINT("Exiting netRecv : type %i, from %i", (SInt32)packet->type, packet->sender);

   return packet;
}

// -- Wrappers

SInt32 Network::netSend(SInt32 dest, PacketType type, const void *buf, UInt32 len)
{
   NetPacket packet;
   assert(_core && _core->getPerformanceModel());
   packet.time = _core->getPerformanceModel()->getCycleCount();
   packet.sender = _core->getId();
   packet.receiver = dest;
   packet.length = len;
   packet.type = type;
   packet.data = buf;

   return netSend(packet);
}

SInt32 Network::netBroadcast(PacketType type, const void *buf, UInt32 len)
{
   return netSend(NetPacket::BROADCAST, type, buf, len);
}

NetPacket* Network::netRecv(SInt32 src, PacketType type)
{
   NetMatch match;
   match.senders.push_back(src);
   match.types.push_back(type);
   return netRecv(match);
}

NetPacket* Network::netRecvFrom(SInt32 src)
{
   NetMatch match;
   match.senders.push_back(src);
   return netRecv(match);
}

NetPacket* Network::netRecvType(PacketType type)
{
   NetMatch match;
   match.types.push_back(type);
   return netRecv(match);
}

void Network::enableModels()
{
   _enabled = true;
   for (int i = 0; i < NUM_STATIC_NETWORKS; i++)
   {
      _models[i]->enable();
   }
}

void Network::disableModels()
{
   _enabled = false;
   for (int i = 0; i < NUM_STATIC_NETWORKS; i++)
   {
      _models[i]->disable();
   }
}

void Network::resetModels()
{
   for (int i = 0; i < NUM_STATIC_NETWORKS; i++)
   {
      _models[i]->reset();
   }
}

// Modeling
UInt32 Network::getModeledLength(const NetPacket& pkt)
{
   UInt32 header_size = 1 + 2 * Config::getSingleton()->getCoreIDLength() + 2;
   if ((pkt.type == SHARED_MEM_1) || (pkt.type == SHARED_MEM_2))
   {
      // packet_type + sender + receiver + length + shmem_msg.size()
      // 1 byte for packet_type
      // log2(core_id) for sender and receiver
      // 2 bytes for packet length
      UInt32 data_size = getCore()->getMemoryManager()->getModeledLength(pkt.data);
      return header_size + data_size;
   }
   else
   {
      return header_size + pkt.length;
   }
}

// -- NetPacket

NetPacket::NetPacket()
   : start_time(0)
   , time(0)
   , type(INVALID_PACKET_TYPE)
   , sender(INVALID_CORE_ID)
   , receiver(INVALID_CORE_ID)
   , length(0)
   , data(0)
   , is_raw(true)
   , sequence_num(0)
   , specific(0)
{
}

NetPacket::NetPacket(UInt64 t, PacketType ty, UInt32 l, const void *d, 
                     bool raw, UInt64 seq_num)
   : start_time(0)
   , time(t)
   , type(ty)
   , sender(INVALID_CORE_ID)
   , receiver(INVALID_CORE_ID)
   , length(l)
   , data(d)
   , is_raw(raw)
   , sequence_num(seq_num)
   , specific(0)
{
}

NetPacket::NetPacket(UInt64 t, PacketType ty, SInt32 s,
                     SInt32 r, UInt32 l, const void *d, 
                     bool raw, UInt64 seq_num)
   : start_time(0)
   , time(t)
   , type(ty)
   , sender(s)
   , receiver(r)
   , length(l)
   , data(d)
   , is_raw(raw)
   , sequence_num(seq_num)
   , specific(0)
{
}


NetPacket::NetPacket(Byte *buffer)
{
   memcpy(this, buffer, sizeof(*this));

   // LOG_ASSERT_ERROR(length > 0, "type(%u), sender(%i), receiver(%i), length(%u)", type, sender, receiver, length);
   if (length > 0)
   {
      Byte* data_buffer = new Byte[length];
      memcpy(data_buffer, buffer + sizeof(*this), length);
      data = data_buffer;
   }

   delete [] buffer;
}

// This implementation is slightly wasteful because there is no need
// to copy the const void* value in the NetPacket when length == 0,
// but I don't see this as a major issue.
UInt32 NetPacket::bufferSize() const
{
   return (sizeof(*this) + length);
}

Byte* NetPacket::makeBuffer() const
{
   UInt32 size = bufferSize();
   assert(size >= sizeof(NetPacket));

   Byte *buffer = new Byte[size];

   memcpy(buffer, this, sizeof(*this));
   memcpy(buffer + sizeof(*this), data, length);

   return buffer;
}

NetPacket* NetPacket::clone() const
{
   assert((data == NULL) == (length == 0));
   
   // Call default copy constructor
   NetPacket* cloned_net_packet = new NetPacket(*this);
   if (length > 0)
   {
      cloned_net_packet->data = new Byte[length];
      memcpy((void*) cloned_net_packet->data, data, length);
   }

   LOG_PRINT("NetPacket: allocate(%p)", cloned_net_packet);
   LOG_PRINT("Data: allocate(%p)", cloned_net_packet->data);
   return cloned_net_packet;
}

void NetPacket::release()
{
   LOG_PRINT("Data: release(%p)", data);
   LOG_PRINT("NetPacket: release(%p)", this);

   assert((data == NULL) == (length == 0));
   if (length > 0)
      delete [] (Byte*) data;
   delete this;
}
