#include <cmath>

#include "core.h"
#include "finite_buffer_network_model.h"
#include "log.h"
#include "packet_type.h"
#include "clock_converter.h"
#include "memory_manager.h"
#include "utils.h"

FiniteBufferNetworkModel::FiniteBufferNetworkModel(Network* net, SInt32 network_id)
   : NetworkModel(net, network_id, true)
   , _enabled(false)
   , _sender_sequence_num(0)
{
   _core_id = getNetwork()->getCore()->getId();
   _flow_control_packet_type = getNetwork()->getPacketTypeFromNetworkId(network_id);

   // Account for Sender Contention Delay
   _sender_contention_model = new QueueModelSimple();

   // Sequence Numbers
   _vec_complete_packet_list.resize(Config::getSingleton()->getTotalCores());
   _vec_next_recv_sequence_num_to_be_assigned.resize(Config::getSingleton()->getTotalCores(), 0);
   _vec_next_recv_sequence_num_to_be_processed.resize(Config::getSingleton()->getTotalCores(), 0);

   // Event Counters
   initializePerformanceCounters();
}

FiniteBufferNetworkModel::~FiniteBufferNetworkModel()
{
   delete _sender_contention_model;
}

void
FiniteBufferNetworkModel::sendNetPacket(NetPacket* raw_packet, list<NetPacket*>& modeling_packet_list_to_send)
{
   ScopedLock sl(_lock);

   LOG_PRINT("sendNetPacket(%i, %p, %u) enter", getNetwork()->getCore()->getId(), raw_packet, _flow_control_scheme);

   assert(getNetwork()->isModeled(*raw_packet));

   // Increment Sequence Number
   raw_packet->sequence_num = _sender_sequence_num ++;
   assert(raw_packet->sender == _core_id);
 
   if (raw_packet->sender == raw_packet->receiver)
      return;

   // Split the packet into multiple flits 
   SInt32 serialization_latency = computeSerializationLatency(raw_packet);
   
   // Account for the Sender Contention Delay
   // TODO: Should there be a contention delay here ?
   UInt64 contention_delay = _sender_contention_model->computeQueueDelay(raw_packet->time, serialization_latency);
   raw_packet->time += contention_delay;
   
   // Divide Packet into Constituent Flits
   FlowControlScheme::dividePacket(_flow_control_scheme,
         raw_packet, modeling_packet_list_to_send,
         serialization_latency);

   // Get the ingress router id
   Router::Id ingress_router_id = computeIngressRouterId(_core_id);

   // Send out all the flits
   list<NetPacket*>::iterator packet_it = modeling_packet_list_to_send.begin();
   for ( ; packet_it != modeling_packet_list_to_send.end(); packet_it ++)
   {
      NetPacket* modeling_packet_to_send = *packet_it;
      modeling_packet_to_send->sender = _core_id;
      modeling_packet_to_send->receiver = ingress_router_id._core_id;
      
      Flit* flit_to_send = (Flit*) modeling_packet_to_send->data;
      flit_to_send->_sender_router_index = CORE_INTERFACE;
      flit_to_send->_receiver_router_index = ingress_router_id._index;
      
   }

   LOG_PRINT("sendNetPacket() exit, modeling_packet_list_to_send.size(%u)", modeling_packet_list_to_send.size());
}

void
FiniteBufferNetworkModel::receiveNetPacket(NetPacket* net_packet,
      list<NetPacket*>& modeling_packet_list_to_send, list<NetPacket*>& raw_packet_list_to_receive)
{
   ScopedLock sl(_lock);

   LOG_PRINT("receiveNetPacket(%p): Time(%llu), Sender(%i), Receiver(%i), Length(%i), Sequence Num(%llu), Raw(%s) enter",
         net_packet, net_packet->time, net_packet->sender, net_packet->receiver,
         net_packet->length, net_packet->sequence_num, (net_packet->is_raw) ? "YES" : "NO");

   if (net_packet->is_raw)
   {
      receiveRawPacket(net_packet, raw_packet_list_to_receive);
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
         receiveModelingPacket(net_packet, raw_packet_list_to_receive); 
      }

      else // (router_idx != Router::Id::CORE_INTERFACE)
      {
         LOG_PRINT("Receiver Router(%i,%i)", net_packet->receiver, network_msg->_receiver_router_index);
         NetworkNode* receiver_network_node = _network_node_map[network_msg->_receiver_router_index];
         LOG_ASSERT_ERROR(receiver_network_node, "Not instantiated router type(%u)", network_msg->_receiver_router_index);

         // TODO: Put this in a routine 
         // Compute Output Endpoint List if "HEAD" flit
         if (network_msg->_type == NetworkMsg::DATA)
         {
            LOG_PRINT("Flit");
            Flit* flit = (Flit*) network_msg;
            if (flit->_type & Flit::HEAD)
            {
               LOG_PRINT("Head Flit");
               HeadFlit* head_flit = (HeadFlit*) flit;
               // Calls the specific network model (emesh, atac, etc.)
               computeOutputEndpointList(head_flit, receiver_network_node);
            }
         }

         // process the 'NetworkMsg'
         receiver_network_node->processNetPacket(net_packet, modeling_packet_list_to_send);
         LOG_PRINT("After Processing: Size of net_packet list(%u)", modeling_packet_list_to_send.size());

         // Print the list - For Debugging
         // printNetPacketList(modeling_packet_list_to_send);
      
      } // if (router_idx == Router::Id::CORE_INTERFACE)

   } // if (net_packet->is_raw)

   LOG_PRINT("receiveNetPacket(%p) exit", net_packet);
}

void
FiniteBufferNetworkModel::receiveRawPacket(NetPacket* raw_packet, list<NetPacket*>& raw_packet_list_to_receive)
{
   LOG_PRINT("receiveRawPacket(%p) enter", raw_packet);
   assert(getNetwork()->isModeled(*raw_packet));

   // 1. Assign a recv_sequence_num to this packet
   UInt32 recv_sequence_num = _vec_next_recv_sequence_num_to_be_assigned[raw_packet->sender] ++;
   UInt64 packet_id = computePacketId(raw_packet->sender, raw_packet->sequence_num);
   _recv_sequence_num_map.insert(make_pair(packet_id, recv_sequence_num));

   // 2. If Local Msg
   if (raw_packet->sender == raw_packet->receiver)
   {
      LOG_PRINT("receiveRawPacket(%p) exit - Local Msg", raw_packet);

      // 2.1 Insert into completePacketList
      insertInCompletePacketList(raw_packet, 0 /* zero_load_delay */);

      // 2.2 Get ready packets
      getReadyPackets(raw_packet->sender, raw_packet_list_to_receive);
      assert(raw_packet_list_to_receive.size() <= 1);
      return;
   }

   // 3. Insert into raw_packet_map
   _recvd_raw_packet_map.insert(make_pair(packet_id, raw_packet));

   // 4. Verify that no modeling packet has been received
   PacketMap::iterator modeling_it = _recvd_modeling_packet_map.find(packet_id);
   assert(modeling_it == _recvd_modeling_packet_map.end());
   
   LOG_PRINT("sender(%i), sequence_num(%llu), packet_id(0x%llx)",
         raw_packet->sender, raw_packet->sequence_num, packet_id);
}

void
FiniteBufferNetworkModel::receiveModelingPacket(NetPacket* modeling_packet, list<NetPacket*>& raw_packet_list_to_receive)
{
   LOG_PRINT("receiveModelingPacket(%p) enter", modeling_packet);
 
   Flit* flit = (Flit*) modeling_packet->data;
   assert(flit->_net_packet == modeling_packet);
   assert(flit->_normalized_time == modeling_packet->time);
   UInt64 packet_id = computePacketId(flit->_sender, modeling_packet->sequence_num);
   
   LOG_PRINT("packet_id(0x%llx)", packet_id);
 
   PacketMap::iterator modeling_it = _recvd_modeling_packet_map.find(packet_id);
   if (modeling_it == _recvd_modeling_packet_map.end())
   {
      LOG_PRINT("New Modeling packet received");
      assert(flit->_type & Flit::HEAD);
      pair<PacketMap::iterator,bool> modeling_it_pair =
            _recvd_modeling_packet_map.insert(make_pair(packet_id,modeling_packet));
      assert(modeling_it_pair.second);
      modeling_it = modeling_it_pair.first;
   }
   else
   {
      LOG_PRINT("Modeling Packet present");
      assert(!(flit->_type & Flit::HEAD));
      NetPacket* prev_modeling_packet = (*modeling_it).second;

      // Remove the data
      LOG_ASSERT_ERROR(modeling_packet->time > prev_modeling_packet->time,
            "time(%llu), previous time(%llu)",
            modeling_packet->time, prev_modeling_packet->time);

      // Release previous modeling packet
      prev_modeling_packet->release();

      (*modeling_it).second = modeling_packet;
   }

   // Check if packet is complete
   if (FlowControlScheme::isPacketComplete(_flow_control_scheme, flit->_type))
   {
      LOG_PRINT("Modeling Packet Complete");
      
      // Find the raw_packet
      PacketMap::iterator raw_it = _recvd_raw_packet_map.find(packet_id);
      assert(raw_it != _recvd_raw_packet_map.end());
      // Get the raw_packet
      NetPacket* raw_packet = (*raw_it).second;
      // Delete the NetPacket from _recvd_raw_packet_map
      _recvd_raw_packet_map.erase(raw_it);

      // Update the NetPacket time information
      raw_packet->time = modeling_packet->time + flit->_num_phits - 1;

      // Add serialization latency to zero load delay
      SInt32 zero_load_delay = flit->_zero_load_delay;
      zero_load_delay += (computeSerializationLatency(raw_packet) - 1);
      
      // Move the raw_packet to the complete packet list
      insertInCompletePacketList(raw_packet, zero_load_delay);

      // Get Ready Packets and return them to the sender
      getReadyPackets(raw_packet->sender, raw_packet_list_to_receive);

      // Delete the modeling packet
      _recvd_modeling_packet_map.erase(modeling_it);
      modeling_packet->release();
      
      LOG_PRINT("receiveModelingPacket(%p) exit - raw packets present", modeling_packet);
   }
}

void
FiniteBufferNetworkModel::insertInCompletePacketList(NetPacket* raw_packet, SInt32 zero_load_delay)
{
   LOG_PRINT("insertInCompletePacketList(%p,%i) enter", raw_packet, zero_load_delay);

   // 1. Extract the recv_sequence_num from the map
   //    1.1. Compute the packet_id of the received raw_packet
   core_id_t sender = raw_packet->sender;
   UInt32 sender_sequence_num = raw_packet->sequence_num;
   UInt64 packet_id = computePacketId(sender, sender_sequence_num);
   //    1.2. Extract the recv_sequence_num using this packet_id
   SequenceNumMap::iterator seq_it = _recv_sequence_num_map.find(packet_id);
   assert(seq_it != _recv_sequence_num_map.end());
   UInt32 recv_sequence_num = (*seq_it).second;
   _recv_sequence_num_map.erase(seq_it);

   // 2. Insert the received raw_packet along with the recv_sequence_num into the complete packet list
   CompletePacketList& complete_packet_list = _vec_complete_packet_list[sender];
   //    2.1. Iterate through the complete_packet_list corresponding to that sender and insert
   //         the packet in the correct position (ascending order of recv_sequence_num)
   for (CompletePacketList::iterator it = complete_packet_list.begin();
                                     it != complete_packet_list.end();
                                     it ++)
   {
      CompletePacket complete_packet = *it;
      LOG_ASSERT_ERROR(recv_sequence_num != complete_packet._recv_sequence_num,
            "Found two packets with the same recv_sequence_num(%u)", recv_sequence_num);
      if (recv_sequence_num < complete_packet._recv_sequence_num)
      {
         // If (my sequence num is less than that of the head of the list),
         // Insert the corresponding packet here
         complete_packet_list.insert(it, CompletePacket(raw_packet, zero_load_delay, recv_sequence_num));
         LOG_PRINT("insertInCompletePacketList(%p,%i) exit", raw_packet, zero_load_delay);
         return;
      }
   }
   
   complete_packet_list.push_back(CompletePacket(raw_packet, zero_load_delay, recv_sequence_num));
   LOG_PRINT("insertInCompletePacketList(%p,%i) exit", raw_packet, zero_load_delay);
}

void
FiniteBufferNetworkModel::getReadyPackets(SInt32 sender, list<NetPacket*>& raw_packet_list_to_receive)
{
   LOG_PRINT("getReadyPackets(%i) enter", sender);
   // The assumption is that all packets are ready at the same time, so if the components
   // inside the core are able to process all of them simultaenously, it will be so
   UInt32& next_recv_sequence_num_to_be_processed = _vec_next_recv_sequence_num_to_be_processed[sender];
   CompletePacketList& complete_packet_list = _vec_complete_packet_list[sender];

   // 3. Extract any ready packets from the complete packet list and hand them over to the higher
   //    layer to be processed
   UInt64 max_packet_time = 0;
   for (CompletePacketList::iterator it = complete_packet_list.begin(); 
                                     it != complete_packet_list.end(); )
   {
      CompletePacket complete_packet = *it;
      
      NetPacket* raw_packet = complete_packet._raw_packet;
      SInt32 zero_load_delay = complete_packet._zero_load_delay;
      UInt32 recv_sequence_num = complete_packet._recv_sequence_num;

      if (recv_sequence_num == next_recv_sequence_num_to_be_processed) // Ready packet
      {
         // Update the time of the raw_packet
         if (max_packet_time == 0)
            max_packet_time = raw_packet->time;
         LOG_ASSERT_ERROR(max_packet_time >= raw_packet->time,
               "Max Packet Time(%llu) < raw_packet->time(%llu)", max_packet_time, raw_packet->time);

         // Update raw packet time - Everyone's time is updated to that at start of queue
         raw_packet->time = max_packet_time;

         // Update Packet Statistics
         updatePacketStatistics(raw_packet, zero_load_delay);

         // Add the packet to the ready packet list
         raw_packet_list_to_receive.push_back(raw_packet);

         // Remove the packet from the complete packet list
         it = complete_packet_list.erase(it);
         
         // Increment next_recv_sequence_num_to_be_processed
         next_recv_sequence_num_to_be_processed ++;
      }
      else // No more packets are ready
      {
         // LOG_PRINT_ERROR("Recv Sequence Number to be processed(%u), Recv Sequence Number(%u), Sender(%i), Receiver(%i)",
         //                  next_recv_sequence_num_to_be_processed, recv_sequence_num, raw_packet->sender, raw_packet->receiver);
         break;
      }
   }
   
   LOG_PRINT("getReadyPackets(%i) exit", sender);
}

void
FiniteBufferNetworkModel::updatePacketStatistics(const NetPacket* raw_packet, SInt32 zero_load_delay)
{
   assert(getNetwork()->isModeled(*raw_packet));

   UInt64 packet_latency = raw_packet->time - raw_packet->start_time;
   
   LOG_ASSERT_ERROR( ((UInt64)zero_load_delay) <= packet_latency,
                    "[Sender(%i), Receiver(%i), Curr Core(%i)] : Zero Load Delay(%i) > Packet Latency(%llu)",
                    raw_packet->sender, raw_packet->receiver, _core_id, zero_load_delay, packet_latency);
   
   UInt64 contention_delay = packet_latency - zero_load_delay;

   _total_packets_received ++;
   UInt32 packet_length = getNetwork()->getModeledLength(*raw_packet);
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

      out << "    Average Packet Length: " << 
         ((float) _total_bytes_received / _total_packets_received) << endl;
      out << "    Average Contention Delay (in clock cycles): " << 
         ((double) _total_contention_delay / _total_packets_received) << endl;
      out << "    Average Contention Delay (in ns): " << 
         ((double) total_contention_delay_in_ns / _total_packets_received) << endl;
      
      out << "    Average Packet Latency (in clock cycles): " <<
         ((double) _total_packet_latency / _total_packets_received) << endl;
      out << "    Average Packet Latency (in ns): " <<
         ((double) total_packet_latency_in_ns / _total_packets_received) << endl;
   }
   else
   {
      out << "    Average Packet Length: 0" << endl;
      out << "    Average Contention Delay (in clock cycles): 0" << endl;
      out << "    Average Contention Delay (in ns): 0" << endl;
      out << "    Average Packet Latency (in clock cycles): 0" << endl;
      out << "    Average Packet Latency (in ns): 0" << endl;
   }
}

SInt32
FiniteBufferNetworkModel::computeSerializationLatency(const NetPacket* raw_packet)
{
   assert(raw_packet->is_raw);
   SInt32 packet_length = getNetwork()->getModeledLength(*raw_packet);

   return (SInt32) ceil((float) (packet_length * 8) / _flit_width);
}

UInt64
FiniteBufferNetworkModel::computePacketId(core_id_t sender, UInt64 sender_sequence_num)
{
   return ( (((UInt64) sender) << 32) + sender_sequence_num );
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
