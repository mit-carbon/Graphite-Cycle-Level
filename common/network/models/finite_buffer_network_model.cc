#include "core.h"
#include "finite_buffer_network_model.h"
#include "log.h"

FiniteBufferNetworkModel::FiniteBufferNetworkModel(Network* net, SInt32 network_id):
   NetworkModel(net, network_id, true),
   _sequence_num(0)
{
   _core_id = getNetwork()->getCore()->getId();
}

FiniteBufferNetworkModel::~FiniteBufferNetworkModel()
{}

void
FiniteBufferNetworkModel::sendNetPacket(NetPacket* net_packet, list<NetPacket*>& net_packet_list_to_send)
{
   assert(net_packet->is_raw);
   net_packet->sequence_num = _sequence_num ++;
 
   // Add the raw packet to the list to send
   net_packet_list_to_send.push_back(net_packet);
 
   assert(net_packet->sender == _core_id); 
   // Split the packet into multiple flits 
   if (net_packet->receiver != net_packet->sender)
   {
      SInt32 packet_length = getNetwork()->getModeledLength(*net_packet);
      FlowControlScheme::dividePacket(_flow_control_scheme, \
            net_packet, net_packet_list_to_send, \
            packet_length, _flit_width);
   }
   list<NetPacket*>::iterator packet_it = net_packet_list_to_send.begin();
   for ( ; packet_it != net_packet_list_to_send.end(); packet_it ++)
   {
      NetPacket* net_packet_to_send = *packet_it;
      net_packet_to_send->sender = _core_id;
      net_packet_to_send->receiver = _core_id;
   }
}

void
FiniteBufferNetworkModel::receiveNetPacket(NetPacket* net_packet, \
      list<NetPacket*>& net_packet_list_to_send, list<NetPacket*>& net_packet_list_to_receive)
{
   if (net_packet->is_raw)
   {
      bool received = receiveRawPacket(net_packet);
      if (received)
         net_packet_list_to_receive.push_back(net_packet);
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
         network_msg->_input_endpoint = receiver_router->getInputEndpointFromRouterId(sender_router_id);
         Flit* flit = (Flit*) network_msg;
         if (flit->_type == Flit::HEAD)
         {
            HeadFlit* head_flit = (HeadFlit*) flit;
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
               net_packet_list_to_send.erase(send_it);
               local_net_packet_list.push_back(net_packet);
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
}

void
FiniteBufferNetworkModel::constructNetPackets(Router* router, \
      vector<NetworkMsg*>& network_msg_list, list<NetPacket*>& net_packet_list)
{
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
                  Flit* new_flit = flit->deepClone();
                  addNetPacketEndpoints(new_flit->_net_packet, sender_router_id, receiver_router_id);
                  new_flit->_net_packet->time = new_time;
                  net_packet_list.push_back(new_flit->_net_packet);
               }

               Router::Id& receiver_router_id = *router_it;
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
   assert(raw_packet->is_raw);

   if (raw_packet->receiver == raw_packet->sender)
      return true;

   UInt64 packet_id = (((UInt64) raw_packet->sender) << 32) + raw_packet->sequence_num;
   
   map<UInt64,NetPacket*>::iterator modeling_it = _received_modeling_packet_map.find(packet_id);
   if (modeling_it == _received_modeling_packet_map.end())
   {
      _received_raw_packet_map.insert(make_pair<UInt64,NetPacket*>(packet_id,raw_packet));
      return false;
   }
   else
   {
      NetPacket* modeling_packet = (*modeling_it).second;
      // Check if Packet is complete
      // For example, if this is tail flit in the wormhole flow control scheme
      if (FlowControlScheme::isPacketComplete(_flow_control_scheme, modeling_packet))
      {
         // All flits have been received
         raw_packet->time = modeling_packet->time;
        
         // Remove the modeling packet 
         _received_modeling_packet_map.erase(modeling_it);
         delete [] (Byte*) modeling_packet->data;
         delete modeling_packet;
         
         return true;
      }
      else
      {
         _received_raw_packet_map.insert(make_pair<UInt64,NetPacket*>(packet_id,raw_packet));
         return false;
      }
   }
}

NetPacket*
FiniteBufferNetworkModel::receiveModelingPacket(NetPacket* modeling_packet)
{
   UInt64 packet_id = (((UInt64) modeling_packet->sender) << 32) + modeling_packet->sequence_num;
  
   map<UInt64,NetPacket*>::iterator modeling_it = _received_modeling_packet_map.find(packet_id);
   if (modeling_it == _received_modeling_packet_map.end())
   {
      pair<map<UInt64,NetPacket*>::iterator,bool> modeling_it_pair = \
            _received_modeling_packet_map.insert(make_pair<UInt64,NetPacket*>(packet_id,modeling_packet));
      assert(modeling_it_pair.second);
      modeling_it = modeling_it_pair.first;
   }
   else
   {
      NetPacket* prev_modeling_packet = (*modeling_it).second;
      assert(!FlowControlScheme::isPacketComplete(_flow_control_scheme, prev_modeling_packet));

      // Remove the data
      assert(modeling_packet->time >= prev_modeling_packet->time);
      delete [] (Byte*) prev_modeling_packet->data;
      delete prev_modeling_packet;

      (*modeling_it).second = modeling_packet;
   }

   // Check if packet is complete
   if (FlowControlScheme::isPacketComplete(_flow_control_scheme, modeling_packet))
   {
      map<UInt64,NetPacket*>::iterator raw_it = _received_raw_packet_map.find(packet_id);
      if (raw_it == _received_raw_packet_map.end())
      {
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
         delete [] (Byte*) modeling_packet->data;
         delete modeling_packet;
         _received_modeling_packet_map.erase(modeling_it);
         
         return raw_packet;
      }
   }

   return (NetPacket*) NULL;
}
