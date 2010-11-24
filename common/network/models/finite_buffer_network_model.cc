#include <cmath>

#include "core.h"
#include "finite_buffer_network_model.h"
#include "log.h"
#include "packet_type.h"
#include "clock_converter.h"
#include "memory_manager_base.h"

FiniteBufferNetworkModel::FiniteBufferNetworkModel(Network* net, SInt32 network_id):
   NetworkModel(net, network_id, true),
   _enabled(false),
   _sequence_num(0)
{
   _core_id = getNetwork()->getCore()->getId();
   // FIXME: Temporary Hack
   _flow_control_packet_type = USER_2;

   initializePerformanceCounters();
}

FiniteBufferNetworkModel::~FiniteBufferNetworkModel()
{}

void
FiniteBufferNetworkModel::sendNetPacket(NetPacket* net_packet, list<NetPacket*>& net_packet_list_to_send)
{
   ScopedLock sl(_lock);

   LOG_PRINT("sendNetPacket(%p) enter", net_packet);
   assert(net_packet->is_raw);

   core_id_t requester = getRequester(*net_packet);
   if ( (!_enabled) || (requester >= (core_id_t) Config::getSingleton()->getApplicationCores()) || \
         (net_packet->sender == net_packet->receiver) )
      return;

   // Increment Sequence Number
   net_packet->sequence_num = _sequence_num ++;
 
   // Add the raw packet to the list to send
   // net_packet_list_to_send.push_back(net_packet);
 
   assert(net_packet->sender == _core_id); 
   // Split the packet into multiple flits 
   SInt32 packet_length = getNetwork()->getModeledLength(*net_packet);
   SInt32 num_flits = computeNumFlits(packet_length);
   FlowControlScheme::dividePacket(_flow_control_scheme, \
         net_packet, net_packet_list_to_send, \
         num_flits, requester);
   
   list<NetPacket*>::iterator packet_it = net_packet_list_to_send.begin();
   for ( ; packet_it != net_packet_list_to_send.end(); packet_it ++)
   {
      NetPacket* net_packet_to_send = *packet_it;
      net_packet_to_send->sender = _core_id;
      net_packet_to_send->receiver = _core_id;
      NetworkMsg* network_msg_to_send = (NetworkMsg*) net_packet_to_send->data;
      network_msg_to_send->_sender_router_index = Router::Id::CORE_INTERFACE;
      // FIXME: EMesh router for now (Make general later, eg. for concentrated mesh)
      network_msg_to_send->_receiver_router_index = 0; 
   }

   LOG_PRINT("sendNetPacket() exit, net_packet_list_to_send.size(%u)", net_packet_list_to_send.size());
}

void
FiniteBufferNetworkModel::receiveNetPacket(NetPacket* net_packet, \
      list<NetPacket*>& net_packet_list_to_send, list<NetPacket*>& net_packet_list_to_receive)
{
   ScopedLock sl(_lock);

   LOG_PRINT("receiveNetPacket(%p) enter", net_packet);

   // Duplicate NetPacket
   if (net_packet->is_raw)
   {
      // Duplicate both packet and data
      NetPacket* cloned_net_packet = net_packet->clone();

      bool received = receiveRawPacket(cloned_net_packet);
      if (received)
         net_packet_list_to_receive.push_back(cloned_net_packet);
   }
   else // (!net_packet->is_raw)
   {
      // get the 'NetworkMsg*' object
      NetworkMsg* network_msg = (NetworkMsg*) net_packet->data;
      Router::Id sender_router_id(net_packet->sender, network_msg->_sender_router_index);
      Router* receiver_router = _router_list[network_msg->_receiver_router_index];
      // populate the '_channel' field in the structure
      // input_channel (for flits)
      // output_channel (for buffer management msgs)
      if (network_msg->_type == NetworkMsg::DATA)
      {
         // Duplicate the NetPacket
         NetPacket* cloned_net_packet = net_packet->clone();
         network_msg = (NetworkMsg*) cloned_net_packet->data;
         Flit* flit = (Flit*) network_msg;
         flit->_net_packet = cloned_net_packet;
         
         // This assumes that each receiving core has at least one router
         flit->_input_endpoint = receiver_router->getInputEndpointFromRouterId(sender_router_id);

         if (flit->_type == Flit::HEAD)
         {
            Flit* head_flit = flit;
            // Calls the specific network model (emesh, atac, etc.)
            computeOutputEndpointList(head_flit, receiver_router);
         }
      }
      else // (network_msg->_type == NetworkMsg::BUFFER_MANGEMENT)
      {
         network_msg->_output_endpoint = receiver_router->getOutputEndpointFromRouterId(sender_router_id);
      }

      // process the 'NetworkMsg'
      vector<NetworkMsg*> network_msg_list;
      receiver_router->processNetworkMsg(network_msg, network_msg_list);

      constructNetPackets(receiver_router, network_msg_list, net_packet_list_to_send);

      // Results in a lot of msgs to be sent to local core and to other cores
      // Separate the msgs sent to the local core
      list<NetPacket*> local_net_packet_list;
      list<NetPacket*>::iterator send_it = net_packet_list_to_send.begin();
      while (send_it != net_packet_list_to_send.end())
      {
         NetPacket* net_packet_to_send = *send_it;
         NetworkMsg* network_msg_to_send = (NetworkMsg*) (net_packet_to_send->data);
         Router::Id recipient(net_packet_to_send->receiver, network_msg_to_send->_receiver_router_index);
         if (network_msg_to_send->_type == NetworkMsg::DATA)
         {
            if (recipient == Router::Id(_core_id, Router::Id::CORE_INTERFACE))
            {
               LOG_PRINT("Local Msg(%p)", net_packet_to_send);
               send_it = net_packet_list_to_send.erase(send_it);
               local_net_packet_list.push_back(net_packet_to_send);
            }
            else
            {
               send_it ++;
            }
         }
         else // (network_msg->_type == NetworkMsg::BUFFER_MANAGEMENT)
         {
            assert(recipient != Router::Id(_core_id, Router::Id::CORE_INTERFACE));
            send_it ++;
         }
      }

      // Handle the local network packets
      list<NetPacket*>::iterator local_it = local_net_packet_list.begin();
      for ( ; local_it != local_net_packet_list.end(); local_it ++)
      {
         NetPacket* local_net_packet = *local_it;
         NetPacket* received_raw_packet = receiveModelingPacket(local_net_packet); 
         if (received_raw_packet)
         {
            net_packet_list_to_receive.push_back(received_raw_packet);
         }
      }
   }
   LOG_PRINT("receiveNetPacket(%p) exit", net_packet);
}

void
FiniteBufferNetworkModel::constructNetPackets(Router* router, \
      vector<NetworkMsg*>& network_msg_list, list<NetPacket*>& net_packet_list)
{
   LOG_PRINT("constructNetPackets(%i,%i) enter", router->getId()._core_id, router->getId()._index);

   Router::Id sender_router_id(router->getId());

   vector<NetworkMsg*>::iterator msg_it = network_msg_list.begin();
   for ( ; msg_it != network_msg_list.end(); msg_it++)
   {
      NetworkMsg* network_msg = *msg_it;
      switch (network_msg->_type)
      {
         case NetworkMsg::DATA:
            
            {
               Flit* flit = (Flit*) network_msg;
               UInt64 new_time = flit->_net_packet->time + \
                                 flit->_normalized_time - flit->_normalized_time_at_entry;

               // Get receiver router id
               vector<Router::Id> receiving_router_id_list;
               if (flit->_output_endpoint._index == Channel::Endpoint::ALL)
               {
                  receiving_router_id_list = router->getRouterIdListFromOutputChannel( \
                        flit->_output_endpoint._channel_id);
               }
               else
               {
                  receiving_router_id_list.push_back(router->getRouterIdFromOutputEndpoint( \
                           flit->_output_endpoint));
               }

               vector<Router::Id>::iterator router_it = receiving_router_id_list.begin();
               for ( ; (router_it + 1) != receiving_router_id_list.end(); router_it ++)
               {
                  Router::Id& receiver_router_id = *router_it;
                  
                  LOG_PRINT("Next Router Id(%i,%i)", \
                        receiver_router_id._core_id, receiver_router_id._index);
                  
                  // Clone NetPacket and Flit
                  NetPacket* cloned_net_packet = flit->_net_packet->clone();
                  Flit* cloned_flit = (Flit*) cloned_net_packet->data;
                  cloned_flit->_net_packet = cloned_net_packet;
                  
                  addNetPacketEndpoints(cloned_net_packet, sender_router_id, receiver_router_id);
                  cloned_net_packet->time = new_time;
                  net_packet_list.push_back(cloned_net_packet);
               }

               Router::Id& receiver_router_id = *router_it;
                  
               LOG_PRINT("Next Router Id(%i,%i)", \
                     receiver_router_id._core_id, receiver_router_id._index);
                  
               addNetPacketEndpoints(flit->_net_packet, sender_router_id, receiver_router_id);
               flit->_net_packet->time = new_time;
               net_packet_list.push_back(flit->_net_packet);
            }

            break;

         case NetworkMsg::BUFFER_MANAGEMENT:
            
            {
               BufferManagementMsg* buffer_msg = (BufferManagementMsg*) network_msg;

               // Create new net_packet struct
               NetPacket* new_net_packet = new NetPacket(buffer_msg->_normalized_time, \
                     _flow_control_packet_type, \
                     buffer_msg->size(), (void*) (buffer_msg), \
                     false /* is_raw*/);
               LOG_PRINT("NetPacket: allocate(%p)", buffer_msg);
               
               // Get receiver router id
               Router::Id& receiver_router_id = \
                     router->getRouterIdFromInputEndpoint(buffer_msg->_input_endpoint);
               addNetPacketEndpoints(new_net_packet, sender_router_id, receiver_router_id);
               net_packet_list.push_back(new_net_packet);
            }

            break;

         default:
            LOG_PRINT_ERROR("Unsupported Msg Type (%u)", network_msg->_type);
            break;
      }
   }
   
   LOG_PRINT("constructNetPackets(%i,%i) exit", router->getId()._core_id, router->getId()._index);
}

void
FiniteBufferNetworkModel::addNetPacketEndpoints(NetPacket* net_packet,
      Router::Id& sender_router_id, Router::Id& receiver_router_id)
{
   NetworkMsg* network_msg = (NetworkMsg*) net_packet->data;

   net_packet->sender = sender_router_id._core_id;
   network_msg->_sender_router_index = sender_router_id._index;
   net_packet->receiver = receiver_router_id._core_id;
   network_msg->_receiver_router_index = receiver_router_id._index;
}

bool
FiniteBufferNetworkModel::receiveRawPacket(NetPacket* raw_packet)
{
   LOG_PRINT("receiveRawPacket(%p) enter", raw_packet);
   assert(raw_packet->is_raw);

   core_id_t requester = getRequester(*raw_packet);
   if ((!_enabled) || (requester >= (core_id_t) Config::getSingleton()->getApplicationCores()) || \
         (raw_packet->sender == raw_packet->receiver))
   {
      LOG_PRINT("receiveRawPacket(%p) exit - Local Msg", raw_packet);
      return true;
   }

   UInt64 packet_id = (((UInt64) raw_packet->sender) << 32) + raw_packet->sequence_num;
  
   LOG_PRINT("sender(%i), sequence_num(%i), packet_id(%llu)", \
         raw_packet->sender, raw_packet->sequence_num, packet_id);

   map<UInt64,NetPacket*>::iterator modeling_it = _received_modeling_packet_map.find(packet_id);
   if (modeling_it == _received_modeling_packet_map.end())
   {
      _received_raw_packet_map.insert(make_pair<UInt64,NetPacket*>(packet_id,raw_packet));
      
      LOG_PRINT("receiveRawPacket(%p) exit - No modeling packet", raw_packet);
      return false;
   }
   else
   {
      LOG_PRINT("Modeling packet present");
      NetPacket* modeling_packet = (*modeling_it).second;
      // Check if Packet is complete
      // For example, if this is tail flit in the wormhole flow control scheme
      if (FlowControlScheme::isPacketComplete(_flow_control_scheme, modeling_packet))
      {
         LOG_PRINT("Modeling packet complete");
         // All flits have been received
         raw_packet->time = modeling_packet->time;
        
         // Remove(/Delete) the modeling packet 
         _received_modeling_packet_map.erase(modeling_it);
         modeling_packet->release();
         
         LOG_PRINT("receiveRawPacket(%p) exit", raw_packet);
         return true;
      }
      else
      {
         LOG_PRINT("Modeling packet incomplete");
         _received_raw_packet_map.insert(make_pair<UInt64,NetPacket*>(packet_id,raw_packet));
         
         LOG_PRINT("receiveRawPacket(%p) exit", raw_packet);
         return false;
      }
   }
}

NetPacket*
FiniteBufferNetworkModel::receiveModelingPacket(NetPacket* modeling_packet)
{
   LOG_PRINT("receiveModelingPacket(%p) enter", modeling_packet);
   
   Flit* flit = (Flit*) modeling_packet->data;
   UInt64 packet_id = (((UInt64) flit->_sender) << 32) + modeling_packet->sequence_num;
   
   LOG_PRINT("packet_id(%llu)", packet_id);
  
   map<UInt64,NetPacket*>::iterator modeling_it = _received_modeling_packet_map.find(packet_id);
   if (modeling_it == _received_modeling_packet_map.end())
   {
      LOG_PRINT("New Modeling packet received");
      pair<map<UInt64,NetPacket*>::iterator,bool> modeling_it_pair = \
            _received_modeling_packet_map.insert(make_pair<UInt64,NetPacket*>(packet_id,modeling_packet));
      assert(modeling_it_pair.second);
      modeling_it = modeling_it_pair.first;
   }
   else
   {
      LOG_PRINT("Modeling Packet present");
      NetPacket* prev_modeling_packet = (*modeling_it).second;
      assert(!FlowControlScheme::isPacketComplete(_flow_control_scheme, prev_modeling_packet));

      // Remove the data
      modeling_packet->time = max<UInt64>(modeling_packet->time, prev_modeling_packet->time);
      // LOG_ASSERT_ERROR(modeling_packet->time > prev_modeling_packet->time,
      //       "time(%llu), previous time(%llu)",
      //       modeling_packet->time, prev_modeling_packet->time);
      prev_modeling_packet->release();

      (*modeling_it).second = modeling_packet;
   }

   // Check if packet is complete
   if (FlowControlScheme::isPacketComplete(_flow_control_scheme, modeling_packet))
   {
      LOG_PRINT("Modeling Packet Complete");
      map<UInt64,NetPacket*>::iterator raw_it = _received_raw_packet_map.find(packet_id);
      if (raw_it == _received_raw_packet_map.end())
      {
         LOG_PRINT("receiveModelingPacket(%p) exit - No raw packet", modeling_packet);
         return (NetPacket*) NULL;
      }
      else
      {
         // Delete the NetPacket from _received_raw_packet_map
         NetPacket* raw_packet = (*raw_it).second;
         _received_raw_packet_map.erase(raw_it);

         // Update the NetPacket time information
         raw_packet->time = modeling_packet->time;

         // Delete the modeling packet
         _received_modeling_packet_map.erase(modeling_it);
         modeling_packet->release();
         
         LOG_PRINT("receiveModelingPacket(%p) exit - raw packet present", modeling_packet);
         return raw_packet;
      }
   }

   LOG_PRINT("receiveModelingPacket(%p) exit - No raw packet", modeling_packet);
   return (NetPacket*) NULL;
}

void
FiniteBufferNetworkModel::processReceivedPacket(NetPacket& packet)
{
   ScopedLock sl(_lock);
   
   core_id_t requester = getRequester(packet);
   if ((!_enabled) || (requester >= (core_id_t) Config::getSingleton()->getApplicationCores()))
      return;

   UInt32 packet_length = getNetwork()->getModeledLength(packet);
   SInt32 num_flits = computeNumFlits(packet_length);

   UInt64 packet_latency = packet.time - packet.start_time;
   UInt64 unloaded_delay = computeUnloadedDelay(packet.sender, _core_id, num_flits);
   assert(unloaded_delay <= packet_latency);
   UInt64 contention_delay = packet_latency - unloaded_delay;

   _total_packets_received ++;
   _total_bytes_received += packet_length;
   _total_packet_latency += packet_latency;
   _total_contention_delay += contention_delay;
}

core_id_t
FiniteBufferNetworkModel::getRequester(const NetPacket& packet)
{
   core_id_t requester = INVALID_CORE_ID;

   if ((packet.type == SHARED_MEM_1) || (packet.type == SHARED_MEM_2))
      requester = getNetwork()->getCore()->getMemoryManager()->getShmemRequester(packet.data);
   else // Other Packet types
      requester = packet.sender;
   
   LOG_ASSERT_ERROR((requester >= 0) && (requester < (core_id_t) Config::getSingleton()->getTotalCores()),
         "requester(%i)", requester);

   return requester;
}

void
FiniteBufferNetworkModel::outputSummary(ostream& out)
{
   out << "    bytes received: " << _total_bytes_received << endl;
   out << "    packets received: " << _total_packets_received << endl;
   if (_total_packets_received > 0)
   {
      UInt64 total_contention_delay_in_ns = convertCycleCount(_total_contention_delay, getFrequency(), 1.0);
      UInt64 total_packet_latency_in_ns = convertCycleCount(_total_packet_latency, getFrequency(), 1.0);

      out << "    average packet length: " << 
         ((float) _total_bytes_received / _total_packets_received) << endl;
      out << "    total contention delay: " << _total_contention_delay << endl;
      out << "    total packet latency: " << _total_packet_latency << endl;
      out << "    average contention delay (in clock cycles): " << 
         ((double) _total_contention_delay / _total_packets_received) << endl;
      out << "    average contention delay (in ns): " << 
         ((double) total_contention_delay_in_ns / _total_packets_received) << endl;
      
      out << "    average packet latency (in clock cycles): " <<
         ((double) _total_packet_latency / _total_packets_received) << endl;
      out << "    average packet latency (in ns): " <<
         ((double) total_packet_latency_in_ns / _total_packets_received) << endl;
   }
   else
   {
      out << "    average packet length: 0" << endl;
      out << "    total contention delay: 0" << endl;
      out << "    total packet latency: 0" << endl;
      
      out << "    average contention delay (in clock cycles): 0" << endl;
      out << "    average contention delay (in ns): 0" << endl;
      out << "    average packet latency (in clock cycles): 0" << endl;
      out << "    average packet latency (in ns): 0" << endl;
   }
}

SInt32
FiniteBufferNetworkModel::computeNumFlits(SInt32 packet_length)
{
   return (SInt32) ceil((float) (packet_length * 8) / _flit_width);
}

void
FiniteBufferNetworkModel::initializePerformanceCounters()
{
   _total_packets_received = 0;
   _total_bytes_received = 0;
   _total_packet_latency = 0;
   _total_contention_delay = 0;
}
