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
   _flow_control_packet_type = getNetwork()->getPacketTypeFromNetworkId(network_id);

   // Account for Sender Contention Delay
   _sender_contention_model = new QueueModelSimple();

   initializePerformanceCounters();
}

FiniteBufferNetworkModel::~FiniteBufferNetworkModel()
{
   delete _sender_contention_model;
}

void
FiniteBufferNetworkModel::sendNetPacket(NetPacket* net_packet, list<NetPacket*>& net_packet_list_to_send)
{
   ScopedLock sl(_lock);

   LOG_PRINT("sendNetPacket(%p) enter", net_packet);
   assert(net_packet->is_raw);

   core_id_t requester = getNetwork()->getRequester(*net_packet);
   assert(getNetwork()->isModeled(*net_packet));

   if (net_packet->sender == net_packet->receiver)
      return;

   // Increment Sequence Number
   net_packet->sequence_num = _sequence_num ++;
 
   assert(net_packet->sender == _core_id);

   // Split the packet into multiple flits 
   SInt32 packet_length = getNetwork()->getModeledLength(*net_packet);
   SInt32 serialization_latency = computeSerializationLatency(packet_length);
   
   // Account for the Sender Contention Delay
   UInt64 contention_delay = _sender_contention_model->computeQueueDelay(net_packet->time, serialization_latency);
   net_packet->time += contention_delay;
   
   // Divide Packet into Constituent Flits
   FlowControlScheme::dividePacket(_flow_control_scheme,
         net_packet, net_packet_list_to_send,
         serialization_latency, requester);

   // Get the ingress router id
   Router::Id ingress_router_id = computeIngressRouterId(_core_id);

   // Send out all the flits
   list<NetPacket*>::iterator packet_it = net_packet_list_to_send.begin();
   for ( ; packet_it != net_packet_list_to_send.end(); packet_it ++)
   {
      NetPacket* net_packet_to_send = *packet_it;
      net_packet_to_send->sender = _core_id;
      net_packet_to_send->receiver = ingress_router_id._core_id;
      
      Flit* flit_to_send = (Flit*) net_packet_to_send->data;
      flit_to_send->_sender_router_index = CORE_INTERFACE;
      flit_to_send->_receiver_router_index = ingress_router_id._index;
      
   }

   LOG_PRINT("sendNetPacket() exit, net_packet_list_to_send.size(%u)", net_packet_list_to_send.size());
}

void
FiniteBufferNetworkModel::receiveNetPacket(NetPacket* net_packet,
      list<NetPacket*>& net_packet_list_to_send, list<NetPacket*>& net_packet_list_to_receive)
{
   ScopedLock sl(_lock);

   LOG_PRINT("receiveNetPacket(%p): Time(%llu), Sender(%i), Receiver(%i), Length(%i), Sequence Num(%llu), Raw(%s) enter",
         net_packet, net_packet->time, net_packet->sender, net_packet->receiver,
         net_packet->length, net_packet->sequence_num, (net_packet->is_raw) ? "YES" : "NO");

   if (net_packet->is_raw)
   {
      bool received = receiveRawPacket(net_packet);
      if (received)
      {
         net_packet_list_to_receive.push_back(net_packet);
         LOG_PRINT("Received Packet");
      }
   }

   else // (!net_packet->is_raw)
   {
      // get the 'NetworkMsg*' object
      NetworkMsg* network_msg = (NetworkMsg*) net_packet->data;
      SInt32 router_idx = network_msg->_receiver_router_index;
      
      if (router_idx == CORE_INTERFACE)
      {
         assert(network_msg->_type == NetworkMsg::DATA);
         // Handle the local network packet
         NetPacket* received_raw_packet = receiveModelingPacket(net_packet); 
         if (received_raw_packet)
         {
            net_packet_list_to_receive.push_back(received_raw_packet);
            LOG_PRINT("Received Packet");
         }
      }

      else // (router_idx != Router::Id::CORE_INTERFACE)
      {
         LOG_PRINT("Receiver Router coreID %i, _network_node_list size = %i", net_packet->receiver, _network_node_list.size()); 
         LOG_ASSERT_ERROR(router_idx >= 0 && router_idx < (SInt32) _network_node_list.size(),
               "Receiver Router Idx(%i), Type(%s)", router_idx, network_msg->getTypeString().c_str());

         LOG_PRINT("Receiver Router(%i,%i)", net_packet->receiver, network_msg->_receiver_router_index);
         NetworkNode* receiver_network_node = _network_node_list[network_msg->_receiver_router_index];
        
         // TODO: Put this in a routine 
         // Compute Output Endpoint List if "HEAD" flit
         if (network_msg->_type == NetworkMsg::DATA)
         {
            LOG_PRINT("Flit");
            Flit* flit = (Flit*) network_msg; 
            if (flit->_type & Flit::HEAD)
            {
               LOG_PRINT("Head Flit");
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
      
      } // if (router_idx == Router::Id::CORE_INTERFACE)

   } // if (net_packet->is_raw)

   LOG_PRINT("receiveNetPacket(%p) exit", net_packet);
}

bool
FiniteBufferNetworkModel::receiveRawPacket(NetPacket* raw_packet)
{
   LOG_PRINT("receiveRawPacket(%p) enter", raw_packet);
   assert(raw_packet->is_raw);
   assert(getNetwork()->isModeled(*raw_packet));
   
   if (raw_packet->sender == raw_packet->receiver)
   {
      LOG_PRINT("receiveRawPacket(%p) exit - Local Msg", raw_packet);
      // Update Packet Statistics
      updatePacketStatistics(*raw_packet, 0 /* zero_load_delay */);
      return true;
   }

   UInt64 packet_id = computePacketId(raw_packet->sender, raw_packet->sequence_num);
  
   LOG_PRINT("sender(%i), sequence_num(%llu), packet_id(0x%llx)",
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

         // Update Packet Receive Statistics
         Flit* flit = (Flit*) modeling_packet->data;
         updatePacketStatistics(*raw_packet, flit->_zero_load_delay);
        
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
      pair<map<UInt64,NetPacket*>::iterator,bool> modeling_it_pair =
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
      // modeling_packet->time = getMax<UInt64>(modeling_packet->time, prev_modeling_packet->time);
      LOG_ASSERT_ERROR(modeling_packet->time > prev_modeling_packet->time,
            "time(%llu), previous time(%llu)",
            modeling_packet->time, prev_modeling_packet->time);
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
         Flit* modeling_flit = (Flit*) modeling_packet->data;
         assert(modeling_flit->_normalized_time == modeling_packet->time); 
         raw_packet->time = modeling_packet->time + modeling_flit->_length - 1;

         // Update Packet Receive Statistics
         Flit* flit = (Flit*) modeling_packet->data;
         updatePacketStatistics(*raw_packet, flit->_zero_load_delay);
        
         // Delete the modeling packet
         _received_modeling_packet_map.erase(modeling_it);
         modeling_packet->release();
         
         LOG_PRINT("receiveModelingPacket(%p) exit - raw packet present", modeling_packet);
         return raw_packet;
      }
   }

   return (NetPacket*) NULL;
}

void
FiniteBufferNetworkModel::updatePacketStatistics(NetPacket& packet, UInt64 zero_load_delay)
{
   assert(packet.is_raw); 
   assert(getNetwork()->isModeled(packet));

   UInt32 packet_length = getNetwork()->getModeledLength(packet);
   SInt32 serialization_latency = computeSerializationLatency(packet_length);

   UInt64 packet_latency = packet.time - packet.start_time;
   
   // Add Serialization Delay
   if (zero_load_delay > 0)
      zero_load_delay += (serialization_latency - 1);
   LOG_ASSERT_ERROR(zero_load_delay <= packet_latency,
                    "[Sender(%i), Receiver(%i), Curr Core(%i)] : Zero Load Delay(%llu) > Packet Latency(%llu)",
                    packet.sender, packet.receiver, _core_id, zero_load_delay, packet_latency);
   
   UInt64 contention_delay = packet_latency - zero_load_delay;

   _total_packets_received ++;
   _total_bytes_received += packet_length;
   _total_packet_latency += packet_latency;
   _total_contention_delay += contention_delay;
}

void
FiniteBufferNetworkModel::outputSummary(ostream& out)
{
   out << "    Bytes Received: " << _total_bytes_received << endl;
   out << "    Packets Received: " << _total_packets_received << endl;
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

   // FIXME: Mispredicted Normalization Requests
   // UInt64 total_mispredicted_normalization_requests = 0;
   // UInt64 total_normalization_requests = 0;
   // for (UInt32 i = 0; i < _network_node_list.size(); i++)
   // {
   //    total_mispredicted_normalization_requests +=
   //          _network_node_list[i]->getTimeNormalizer()->getTotalMispredicted();
   //    total_normalization_requests += _network_node_list[i]->getTimeNormalizer()->getTotalRequests();
   // }
   // out << "    mispredicted normalization requests (%): "
   //     << ((double) total_mispredicted_normalization_requests) * 100 / total_normalization_requests 
   //     << endl;
}

SInt32
FiniteBufferNetworkModel::computeSerializationLatency(SInt32 packet_length)
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
FiniteBufferNetworkModel::printNetPacketList(const list<NetPacket*>& net_packet_list) const
{
   list<NetPacket*>::const_iterator packet_it = net_packet_list.begin();
   for ( ; packet_it != net_packet_list.end(); packet_it ++)
   {
      NetPacket* net_packet = *packet_it;
      NetworkMsg* network_msg = (NetworkMsg*) (net_packet->data);
      Router::Id recipient(net_packet->receiver, network_msg->_receiver_router_index);
      if (network_msg->_type == NetworkMsg::DATA)
      {
         Flit* flit = (Flit*) network_msg;
         LOG_PRINT("Flit: Time(%llu), Type(%s), Sender(%i), Receiver(%i), Output Endpoint(%i,%i), Sequence Num(%llu)",
               flit->_normalized_time, flit->getTypeString().c_str(),
               flit->_sender, flit->_receiver,
               flit->_output_endpoint._channel_id, flit->_output_endpoint._index,
               flit->_net_packet->sequence_num);
      }
      else // (network_msg->_type == NetworkMsg::BUFFER_MANAGEMENT)
      {
         BufferManagementMsg* buffer_msg = (BufferManagementMsg*) network_msg;
         LOG_PRINT("Buffer Management: Time(%llu)", buffer_msg->_normalized_time);
         assert(recipient != Router::Id(_core_id, CORE_INTERFACE));
      }
   } // for ( ; packet_it != net_packet_list.end(); packet_it ++)
}
