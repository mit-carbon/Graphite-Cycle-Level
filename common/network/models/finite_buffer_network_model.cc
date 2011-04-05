#include <cmath>

#include "core.h"
#include "finite_buffer_network_model.h"
#include "log.h"
#include "packet_type.h"
#include "clock_converter.h"
#include "memory_manager.h"
#include "utils.h"

FiniteBufferNetworkModel::FiniteBufferNetworkModel(Network* net, SInt32 network_id):
   NetworkModel(net, network_id, true),
   _enabled(false),
   _sequence_num(0)
{
   _core_id = getNetwork()->getCore()->getId();
   // FIXME: Temporary Hack - Make this more general
   _flow_control_packet_type = USER_2;

   initializePerformanceCounters();
}

FiniteBufferNetworkModel::~FiniteBufferNetworkModel()
{}

void
FiniteBufferNetworkModel::sendNetPacket(NetPacket* net_packet, list<NetPacket*>& net_packet_list_to_send)
{
   ScopedLock sl(_lock);

   LOG_ASSERT_ERROR(net_packet->receiver != NetPacket::BROADCAST,
         "Does not work yet for broadcast");

   LOG_PRINT("sendNetPacket(%p) enter", net_packet);
   assert(net_packet->is_raw);

   core_id_t requester = getNetwork()->getRequester(*net_packet);
   if ( (!getNetwork()->isModeled(*net_packet)) || (net_packet->sender == net_packet->receiver) )
      return;

   // Increment Sequence Number
   net_packet->sequence_num = _sequence_num ++;
 
   assert(net_packet->sender == _core_id); 
   // Split the packet into multiple flits 
   SInt32 packet_length = getNetwork()->getModeledLength(*net_packet);
   SInt32 num_flits = computeNumFlits(packet_length);
   FlowControlScheme::dividePacket(_flow_control_scheme,
         net_packet, net_packet_list_to_send,
         num_flits, requester);

   list<NetPacket*>::iterator packet_it = net_packet_list_to_send.begin();
   for (UInt32 flit_num = 0;
        packet_it != net_packet_list_to_send.end();
        packet_it ++, flit_num ++)
   {
      NetPacket* net_packet_to_send = *packet_it;
      net_packet_to_send->sender = _core_id;
      net_packet_to_send->receiver = _core_id;
      
      Flit* flit_to_send = (Flit*) net_packet_to_send->data;
      flit_to_send->_sender_router_index = Router::Id::CORE_INTERFACE;
      // FIXME: EMesh router for now (Make general later, eg. for concentrated mesh and other topologies)
      flit_to_send->_receiver_router_index = 0;
   }

   LOG_PRINT("sendNetPacket() exit, net_packet_list_to_send.size(%u)", net_packet_list_to_send.size());
}

void
FiniteBufferNetworkModel::receiveNetPacket(NetPacket* net_packet,
      list<NetPacket*>& net_packet_list_to_send, list<NetPacket*>& net_packet_list_to_receive)
{
   ScopedLock sl(_lock);

   LOG_PRINT("receiveNetPacket(%p): Time(%llu), Sender(%i), Receiver(%i), Length(%i), Sequence Num(%llu), Raw(%s) enter", net_packet, net_packet->time, net_packet->sender, net_packet->receiver, net_packet->length, net_packet->sequence_num, (net_packet->is_raw) ? "YES" : "NO");

   // Duplicate NetPacket
   if (net_packet->is_raw)
   {
      // Duplicate both packet and data
      NetPacket* cloned_net_packet = net_packet->clone();

      bool received = receiveRawPacket(cloned_net_packet);
      if (received)
      {
         net_packet_list_to_receive.push_back(cloned_net_packet);
         LOG_PRINT("Received Packet");
      }
   }
   else // (!net_packet->is_raw)
   {
      // get the 'NetworkMsg*' object
      NetworkMsg* network_msg = (NetworkMsg*) net_packet->data;
      NetworkNode* receiver_network_node = _network_node_list[network_msg->_receiver_router_index];
     
      // TODO: Put this in a routine 
      // Compute Output Endpoint List if "HEAD" flit
      if (network_msg->_type == NetworkMsg::DATA)
      {
         Flit* flit = (Flit*) network_msg; 
         if (flit->_type == Flit::HEAD)
         {
            Flit* head_flit = flit;
            // Calls the specific network model (emesh, atac, etc.)
            computeOutputEndpointList(head_flit, receiver_network_node);
         }
      }

      // process the 'NetworkMsg'
      receiver_network_node->processNetPacket(net_packet, net_packet_list_to_send);
      LOG_PRINT("After Processing: Size of net_packet list(%u)", net_packet_list_to_send.size());

      // Print the list - For Debugging
      // printNetPacketList(net_packet_list_to_send);

      // Results in a lot of msgs to be sent to local core and to other cores
      // Separate the msgs sent to the local core
      list<NetPacket*> local_net_packet_list;
      list<NetPacket*>::iterator packet_it = net_packet_list_to_send.begin();
      while (packet_it != net_packet_list_to_send.end())
      {
         NetPacket* net_packet_to_send = *packet_it;
         NetworkMsg* network_msg_to_send = (NetworkMsg*) (net_packet_to_send->data);
         Router::Id recipient(net_packet_to_send->receiver, network_msg_to_send->_receiver_router_index);
         if (network_msg_to_send->_type == NetworkMsg::DATA)
         {
            Flit* flit = (Flit*) network_msg_to_send; 
            LOG_PRINT("Flit: Time(%llu), Type(%s), Sender(%i), Receiver(%i), Output Endpoint(%i,%i), Sequence Num(%llu)", \
                  flit->_net_packet->time, (flit->getTypeString()).c_str(), \
                  flit->_sender, flit->_receiver, \
                  flit->_output_endpoint._channel_id, flit->_output_endpoint._index, \
                  flit->_net_packet->sequence_num);
            
            if (recipient == Router::Id(_core_id, Router::Id::CORE_INTERFACE))
            {
               LOG_PRINT("Flit: Local");
               packet_it = net_packet_list_to_send.erase(packet_it);
               local_net_packet_list.push_back(net_packet_to_send);
            }
            else
            {
               packet_it ++;
            }
         }
         else // (network_msg_to_send->_type == NetworkMsg::BUFFER_MANAGEMENT)
         {
            LOG_PRINT("Buffer Management: Time(%llu)", net_packet_to_send->time);
            assert(recipient != Router::Id(_core_id, Router::Id::CORE_INTERFACE));
            packet_it ++;
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
            LOG_PRINT("Received Packet");
         }
      }
   }
   LOG_PRINT("receiveNetPacket(%p) exit", net_packet);
}

bool
FiniteBufferNetworkModel::receiveRawPacket(NetPacket* raw_packet)
{
   LOG_PRINT("receiveRawPacket(%p) enter", raw_packet);
   assert(raw_packet->is_raw);

   if ( (!getNetwork()->isModeled(*raw_packet)) || (raw_packet->sender == raw_packet->receiver) )
   {
      LOG_PRINT("receiveRawPacket(%p) exit - Local Msg", raw_packet);
      return true;
   }

   UInt64 packet_id = computePacketId(raw_packet->sender, raw_packet->sequence_num);
  
   LOG_PRINT("sender(%i), sequence_num(%llu), packet_id(0x%llx)", \
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
   assert(flit->_net_packet == modeling_packet);
   UInt64 packet_id = computePacketId(flit->_sender, modeling_packet->sequence_num);
   
   LOG_PRINT("packet_id(0x%llx)", packet_id);
 
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

      // TODO: Check this properly
      // Remove the data
      modeling_packet->time = getMax<UInt64>(modeling_packet->time, prev_modeling_packet->time);
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
   
   if (!getNetwork()->isModeled(packet))
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
      out << "    average contention delay (in clock cycles): 0" << endl;
      out << "    average contention delay (in ns): 0" << endl;
      out << "    average packet latency (in clock cycles): 0" << endl;
      out << "    average packet latency (in ns): 0" << endl;
   }

   // Mispredicted Normalization Requests
   UInt64 total_mispredicted_normalization_requests = 0;
   UInt64 total_normalization_requests = 0;
   for (UInt32 i = 0; i < _network_node_list.size(); i++)
   {
      total_mispredicted_normalization_requests += \
            _network_node_list[i]->getTimeNormalizer()->getTotalMispredicted();
      total_normalization_requests += _network_node_list[i]->getTimeNormalizer()->getTotalRequests();
   }
   out << "    mispredicted normalization requests (%): "
       << ((double) total_mispredicted_normalization_requests) * 100 / total_normalization_requests 
       << endl;
}

SInt32
FiniteBufferNetworkModel::computeNumFlits(SInt32 packet_length)
{
   return (SInt32) ceil((float) (packet_length * 8) / _flit_width);
}

UInt64
FiniteBufferNetworkModel::computePacketId(core_id_t sender, UInt64 sequence_num)
{
   return ( (((UInt64) sender) << 32) + sequence_num );
}

void
FiniteBufferNetworkModel::initializePerformanceCounters()
{
   _total_packets_received = 0;
   _total_bytes_received = 0;
   _total_packet_latency = 0;
   _total_contention_delay = 0;
}

void
FiniteBufferNetworkModel::printNetPacketList(const list<NetPacket*>& net_packet_list)
{
   list<NetPacket*>::const_iterator packet_it = net_packet_list.begin();
   for ( ; packet_it != net_packet_list.end(); packet_it ++)
   {
      NetPacket* net_packet = *packet_it;
      NetworkMsg* network_msg = (NetworkMsg*) net_packet->data;
      printf("%s - Time(%llu): Sender(%i,%i), Receiver(%i,%i)\n", \
            (network_msg->getTypeString()).c_str(), \
            (long long unsigned int) network_msg->_normalized_time, \
            net_packet->sender, network_msg->_sender_router_index, \
            net_packet->receiver, network_msg->_receiver_router_index);
   }
}
