#include "network_node.h"
#include "utils.h"
#include "log.h"

NetworkNode::NetworkNode(Router::Id router_id, \
      UInt32 flit_width, \
      RouterPerformanceModel* router_performance_model, \
      RouterPowerModel* router_power_model, \
      vector<LinkPerformanceModel*>& link_performance_model_list, \
      vector<LinkPowerModel*>& link_power_model_list, \
      vector<vector<Router::Id> >& input_channel_to_router_id_list__mapping, \
      vector<vector<Router::Id> >& output_channel_to_router_id_list__mapping):
   _router_id(router_id),
   _flit_width(flit_width),
   _router_performance_model(router_performance_model),
   _router_power_model(router_power_model),
   _link_performance_model_list(link_performance_model_list),
   _link_power_model_list(link_power_model_list),
   _input_channel_to_router_id_list__mapping(input_channel_to_router_id_list__mapping),
   _output_channel_to_router_id_list__mapping(output_channel_to_router_id_list__mapping)
{
   assert(_router_performance_model);
   createMappings();

   // Time Normalizer
   _time_normalizer = TimeNormalizer::create(Config::getSingleton()->getApplicationCores());
}

NetworkNode::~NetworkNode()
{
   delete _time_normalizer;
}

void
NetworkNode::processNetPacket(NetPacket* input_net_packet, list<NetPacket*>& output_net_packet_list)
{
   LOG_PRINT("processNetPacket(%p, %p) enter", this, input_net_packet);

   NetworkMsg* input_network_msg = (NetworkMsg*) input_net_packet->data;
   vector<NetworkMsg*> output_network_msg_list;

   Router::Id sender_router_id(input_net_packet->sender, input_network_msg->_sender_router_index);
   Router::Id receiver_router_id(input_net_packet->receiver, input_network_msg->_receiver_router_index);
   assert(receiver_router_id == getRouterId());
   
   switch (input_network_msg->_type)
   {
   
   case NetworkMsg::DATA:

      {
         // Duplicate the NetPacket
         NetPacket* cloned_net_packet = input_net_packet->clone();
         input_network_msg = (NetworkMsg*) cloned_net_packet->data;
         Flit* flit = (Flit*) input_network_msg;
         flit->_net_packet = cloned_net_packet;
         
         // input_endpoint (for flits)
         flit->_input_endpoint = getInputEndpointFromRouterId(sender_router_id);
         
         LOG_PRINT("Flit: Time(%llu), Type(%s), Sender(%i), Receiver(%i), Input Endpoint(%i,%i), Sequence Num(%llu)", \
               flit->_net_packet->time, (flit->getTypeString()).c_str(), \
               flit->_sender, flit->_receiver, \
               flit->_input_endpoint._channel_id, flit->_input_endpoint._index, \
               flit->_net_packet->sequence_num);

         normalizeTime(flit);

         _router_performance_model->processDataMsg(flit, output_network_msg_list);
      }

      break;

   case NetworkMsg::BUFFER_MANAGEMENT:

      {
         BufferManagementMsg* buffer_msg = (BufferManagementMsg*) input_network_msg;
 
         // output_endpoint (for buffer management msgs)
         buffer_msg->_output_endpoint = getOutputEndpointFromRouterId(sender_router_id);
 
         LOG_PRINT("Buffer Management: Output Endpoint(%i,%i)", \
               buffer_msg->_output_endpoint._channel_id, buffer_msg->_output_endpoint._index);

         normalizeTime(buffer_msg);

         // Add Credit Pipeline Delay
         buffer_msg->_normalized_time += _router_performance_model->getCreditPipelineDelay();

         _router_performance_model->processBufferManagementMsg(buffer_msg, output_network_msg_list);
      }

      break;

   default:
      LOG_PRINT_ERROR("Unrecognized network msg type(%u)", input_network_msg->_type);
      break;

   }

   vector<NetworkMsg*>::iterator msg_it = output_network_msg_list.begin();
   for ( ; msg_it != output_network_msg_list.end(); msg_it ++)
   {
      NetworkMsg* output_network_msg = *msg_it;
      // Account for router and link delays + update dynamic energy
      performRouterAndLinkTraversal(output_network_msg);
      
      // Set rate of progress info
      if (output_network_msg->_type == NetworkMsg::BUFFER_MANAGEMENT)
         communicateProgressRateInfo((BufferManagementMsg*) output_network_msg);

      // Construct NetPacket and add to list
      constructNetPackets(output_network_msg, output_net_packet_list);
   }
 
   LOG_PRINT("processNetPacket(%p, %p) exit", this, input_net_packet);
}

void
NetworkNode::constructNetPackets(NetworkMsg* network_msg, list<NetPacket*>& net_packet_list)
{
   switch (network_msg->_type)
   {

   case NetworkMsg::DATA:
      
      {
         Flit* flit = (Flit*) network_msg;

         flit->_net_packet->time += (flit->_normalized_time - flit->_normalized_time_at_entry);

         // Get receiver router id
         vector<Router::Id> receiving_router_id_list;
         if (flit->_output_endpoint._index == Channel::Endpoint::ALL)
         {
            receiving_router_id_list = getRouterIdListFromOutputChannel(flit->_output_endpoint._channel_id);
         }
         else
         {
            receiving_router_id_list.push_back(getRouterIdFromOutputEndpoint(flit->_output_endpoint));
         }

         LOG_PRINT("Flit: Time(%llu), Type(%s), Output Endpoint(%i,%i)", \
               (long long unsigned int) flit->_net_packet->time, \
               (flit->getTypeString()).c_str(), \
               flit->_output_endpoint._channel_id, flit->_output_endpoint._index);

         vector<Router::Id>::iterator router_it = receiving_router_id_list.begin();
         for ( ; (router_it + 1) != receiving_router_id_list.end(); router_it ++)
         {
            Router::Id& receiver_router_id = *router_it;
            LOG_PRINT("Flit: Next Router Id(%i,%i)", \
                  receiver_router_id._core_id, receiver_router_id._index);
            
            // Clone NetPacket and Flit
            NetPacket* cloned_net_packet = flit->_net_packet->clone();
            Flit* cloned_flit = (Flit*) cloned_net_packet->data;
            cloned_flit->_net_packet = cloned_net_packet;
            
            addNetPacketEndpoints(cloned_net_packet, getRouterId(), receiver_router_id);
            net_packet_list.push_back(cloned_net_packet);
         }

         Router::Id& receiver_router_id = *router_it;
         LOG_PRINT("Flit: Next Router Id(%i,%i)", \
               receiver_router_id._core_id, receiver_router_id._index);
            
         addNetPacketEndpoints(flit->_net_packet, getRouterId(), receiver_router_id);
         net_packet_list.push_back(flit->_net_packet);
      }

      break;

   case NetworkMsg::BUFFER_MANAGEMENT:
      
      {
         BufferManagementMsg* buffer_msg = (BufferManagementMsg*) network_msg;

         // FIXME: Make this general
         PacketType flow_control_packet_type = USER_2;

         // Create new net_packet struct
         NetPacket* new_net_packet = new NetPacket(buffer_msg->_normalized_time /* time */,
               flow_control_packet_type,
               buffer_msg->size(), (void*) (buffer_msg),
               false /* is_raw*/);
        
         LOG_PRINT("Buffer Management: Time(%llu), Input Endpoint(%i,%i)", \
               (long long unsigned int) new_net_packet->time, \
               buffer_msg->_input_endpoint._channel_id, buffer_msg->_input_endpoint._index);
         
         // Get receiver router id
         Router::Id& receiver_router_id = getRouterIdFromInputEndpoint(buffer_msg->_input_endpoint);
         LOG_PRINT("Buffer Management: Next Router Id(%i,%i)", \
               receiver_router_id._core_id, receiver_router_id._index);

         addNetPacketEndpoints(new_net_packet, getRouterId(), receiver_router_id);
         net_packet_list.push_back(new_net_packet);
      }

      break;

   default:
      LOG_PRINT_ERROR("Unsupported Msg Type (%u)", network_msg->_type);
      break;

   }
}

void
NetworkNode::addNetPacketEndpoints(NetPacket* net_packet,
      Router::Id sender_router_id, Router::Id receiver_router_id)
{
   NetworkMsg* network_msg = (NetworkMsg*) net_packet->data;

   net_packet->sender = sender_router_id._core_id;
   network_msg->_sender_router_index = sender_router_id._index;
   net_packet->receiver = receiver_router_id._core_id;
   network_msg->_receiver_router_index = receiver_router_id._index;
}

void
NetworkNode::performRouterAndLinkTraversal(NetworkMsg* output_network_msg)
{
   switch (output_network_msg->_type)
   {
      case NetworkMsg::DATA:
         
         {
            Flit* flit = (Flit*) output_network_msg;
            SInt32 output_channel = flit->_output_endpoint._channel_id;

            // Router Performance Model (Data Pipeline delay)
            flit->_normalized_time += _router_performance_model->getDataPipelineDelay();
            
            // Router Power Model
            if (_router_power_model)
               _router_power_model->updateDynamicEnergy(_flit_width/2, flit->_length);
            
            // Link Performance Model (Link delay)
            if (_link_performance_model_list[output_channel])
               flit->_normalized_time += _link_performance_model_list[output_channel]->getDelay();
            
            // Link Power Model
            if (_link_power_model_list[output_channel])
               _link_power_model_list[output_channel]->updateDynamicEnergy(_flit_width/2, flit->_length);
         }
         
         break;

      case NetworkMsg::BUFFER_MANAGEMENT:
         
         {
            BufferManagementMsg* buffer_msg = (BufferManagementMsg*) output_network_msg;
            SInt32 input_channel = buffer_msg->_input_endpoint._channel_id;

            // FIXME: No power modeling for the buffer management messages
            
            // Link Performance Model
            if (_link_performance_model_list[input_channel])
               buffer_msg->_normalized_time += _link_performance_model_list[input_channel]->getDelay();
         }
         
         break;

      default:
         LOG_PRINT_ERROR("Unrecognized NetworkMsg Type(%u)", output_network_msg->_type);
         break;
   }
}

void
NetworkNode::normalizeTime(NetworkMsg* network_msg)
{
   switch (network_msg->_type)
   {
      case NetworkMsg::DATA:

         {
            Flit* flit = (Flit*) network_msg;
            // Compute normalized time
            flit->_normalized_time = flit->_net_packet->time; 
            // _time_normalizer->normalize(flit->_net_packet->time, flit->_requester);
            // Set the entry time to account for the time spent in the router
            flit->_normalized_time_at_entry = flit->_normalized_time;
            LOG_PRINT("Normalize(DATA, %llu) -> %llu", flit->_net_packet->time, flit->_normalized_time);
         }

         break;

      case NetworkMsg::BUFFER_MANAGEMENT:
         
         {            
            BufferManagementMsg* buffer_msg = (BufferManagementMsg*) network_msg;
            // Re-normalize according to average rate of progress
            UInt64 renormalized_time = buffer_msg->_normalized_time;
            // _time_normalizer->renormalize(buffer_msg->_normalized_time, buffer_msg->_average_rate_of_progress);
            LOG_PRINT("Renormalize(BUFFER, %llu) -> %llu", buffer_msg->_normalized_time, renormalized_time);
            buffer_msg->_normalized_time = renormalized_time;
         }

         break;

      default:
         LOG_PRINT_ERROR("Unrecognized NetworkMsg type(%u)", network_msg->_type);
         break;
   }
}

void
NetworkNode::communicateProgressRateInfo(BufferManagementMsg* buffer_msg)
{
   // Set the average rate of progress
   // buffer_msg->_average_rate_of_progress = _time_normalizer->getAverageRateOfProgress();
}

void
NetworkNode::addChannelMapping(vector<vector<Router::Id> >& channel_to_router_id_list__mapping, \
      Router::Id& router_id)
{
   vector<Router::Id> router_id_list(1, router_id);
   channel_to_router_id_list__mapping.push_back(router_id_list);
}

void
NetworkNode::addChannelMapping(vector<vector<Router::Id> >& channel_to_router_id_list__mapping, \
      vector<Router::Id>& router_id_list)
{
   channel_to_router_id_list__mapping.push_back(router_id_list);
}

void
NetworkNode::createMappings()
{
   for (SInt32 i = 0; i < (SInt32) _input_channel_to_router_id_list__mapping.size(); i++)
   {
      for (SInt32 j = 0; j < (SInt32) _input_channel_to_router_id_list__mapping[i].size(); j++)
      {
         _router_id_to_input_endpoint_mapping.insert( \
               make_pair<Router::Id, Channel::Endpoint>( \
                  Router::Id(_input_channel_to_router_id_list__mapping[i][j]), \
                  Channel::Endpoint(i,j) \
               ) \
            );
      }
   }

   for (SInt32 i = 0; i < (SInt32) _output_channel_to_router_id_list__mapping.size(); i++)
   {
      for (SInt32 j = 0; j < (SInt32) _output_channel_to_router_id_list__mapping[i].size(); j++)
      {
         _router_id_to_output_endpoint_mapping.insert( \
               make_pair<Router::Id, Channel::Endpoint>( \
                  Router::Id(_output_channel_to_router_id_list__mapping[i][j]), \
                  Channel::Endpoint(i,j) \
               ) \
            );
      }
   }
}

Channel::Endpoint&
NetworkNode::getInputEndpointFromRouterId(Router::Id& router_id)
{
   assert(_router_id_to_input_endpoint_mapping.find(router_id) != _router_id_to_input_endpoint_mapping.end());
   return _router_id_to_input_endpoint_mapping[router_id];
}

Channel::Endpoint&
NetworkNode::getOutputEndpointFromRouterId(Router::Id& router_id)
{
   assert(_router_id_to_output_endpoint_mapping.find(router_id) != _router_id_to_output_endpoint_mapping.end());
   return _router_id_to_output_endpoint_mapping[router_id];
}

Router::Id&
NetworkNode::getRouterIdFromInputEndpoint(Channel::Endpoint& input_endpoint)
{
   assert( (input_endpoint._channel_id >= 0) && (input_endpoint._channel_id < (SInt32) _input_channel_to_router_id_list__mapping.size()) );
   vector<Router::Id>& router_id_list = _input_channel_to_router_id_list__mapping[input_endpoint._channel_id];
   
   assert( (input_endpoint._index >= 0) && (input_endpoint._index < (SInt32) router_id_list.size()) );
   return router_id_list[input_endpoint._index];
}

Router::Id&
NetworkNode::getRouterIdFromOutputEndpoint(Channel::Endpoint& output_endpoint)
{
   assert( (output_endpoint._channel_id >= 0) && (output_endpoint._channel_id < (SInt32) _output_channel_to_router_id_list__mapping.size()) );
   vector<Router::Id>& router_id_list = _output_channel_to_router_id_list__mapping[output_endpoint._channel_id];
 
   assert( (output_endpoint._index >= 0) && (output_endpoint._index < (SInt32) router_id_list.size()) );
   return router_id_list[output_endpoint._index];
}

vector<Router::Id>&
NetworkNode::getRouterIdListFromOutputChannel(SInt32 output_channel_id)
{
   assert( (output_channel_id >= 0) && (output_channel_id < (SInt32) _output_channel_to_router_id_list__mapping.size()) );
   return _output_channel_to_router_id_list__mapping[output_channel_id];
}
