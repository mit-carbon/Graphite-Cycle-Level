#include <sstream>
using std::ostringstream;
#include "network_node.h"
#include "utils.h"
#include "credit_msg.h"
#include "on_off_msg.h"
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
   _output_channel_to_router_id_list__mapping(output_channel_to_router_id_list__mapping),
   _last_net_packet_time(0)
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
   LOG_ASSERT_ERROR(input_net_packet->time >= _last_net_packet_time,
         "Curr Net Packet Time(%llu), Last Net Packet Time(%llu)", input_net_packet->time, _last_net_packet_time);
   _last_net_packet_time = input_net_packet->time;

   LOG_PRINT("#########################################################");
   printNetPacket(input_net_packet, true);
   LOG_PRINT("=========================================================");

   NetworkMsg* input_network_msg = (NetworkMsg*) input_net_packet->data;
   vector<NetworkMsg*> output_network_msg_list;

   Router::Id sender_router_id(input_net_packet->sender, input_network_msg->_sender_router_index);
   Router::Id receiver_router_id(input_net_packet->receiver, input_network_msg->_receiver_router_index);
   assert(receiver_router_id == getRouterId());
   
   switch (input_network_msg->_type)
   {
   
   case NetworkMsg::DATA:

      {
         Flit* flit = (Flit*) input_network_msg;
         flit->_net_packet = input_net_packet;
         
         // input_endpoint (for flits)
         flit->_input_endpoint = getInputEndpointFromRouterId(sender_router_id);
        
         // Normalize Time 
         normalizeTime(flit);

         _router_performance_model->processDataMsg(flit, output_network_msg_list);
      }

      break;

   case NetworkMsg::BUFFER_MANAGEMENT:

      {
         BufferManagementMsg* buffer_msg = (BufferManagementMsg*) input_network_msg;
 
         // output_endpoint (for buffer management msgs)
         buffer_msg->_output_endpoint = getOutputEndpointFromRouterId(sender_router_id);

         // Normalize Time 
         normalizeTime(buffer_msg);

         // Perform Router and Link Traversal
         performRouterAndLinkTraversal(buffer_msg);

         _router_performance_model->processBufferManagementMsg(buffer_msg, output_network_msg_list);

         // Delete the NetPacket
         input_net_packet->release();
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
      
      if (output_network_msg->_type == NetworkMsg::DATA)
      {
         // Account for router and link delays + update dynamic energy
         performRouterAndLinkTraversal(output_network_msg);
      }
      else if (output_network_msg->_type == NetworkMsg::BUFFER_MANAGEMENT)
      {
         // Set rate of progress info
         communicateProgressRateInfo((BufferManagementMsg*) output_network_msg);
      }

      // Construct NetPacket and add to list
      constructNetPackets(output_network_msg, output_net_packet_list);
   }

   for (list<NetPacket*>::iterator it = output_net_packet_list.begin();
         it != output_net_packet_list.end(); it ++)
   {
      if (it != output_net_packet_list.begin())
         LOG_PRINT("_________________________________________________________");
      printNetPacket(*it);
   }
   LOG_PRINT("#########################################################");
}

void
NetworkNode::constructNetPackets(NetworkMsg* network_msg, list<NetPacket*>& net_packet_list)
{
   LOG_PRINT("constructNetPackets(%p) start", network_msg);
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

         vector<Router::Id>::iterator router_it = receiving_router_id_list.begin();
         for ( ; (router_it + 1) != receiving_router_id_list.end(); router_it ++)
         {
            Router::Id& receiver_router_id = *router_it;
            
            // Clone NetPacket and Flit
            NetPacket* cloned_net_packet = flit->_net_packet->clone();
            Flit* cloned_flit = (Flit*) cloned_net_packet->data;
            cloned_flit->_net_packet = cloned_net_packet;
            
            addNetPacketEndpoints(cloned_net_packet, getRouterId(), receiver_router_id);
            net_packet_list.push_back(cloned_net_packet);
         }

         Router::Id& receiver_router_id = *router_it;
 
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
        
         // Get receiver router id
         Router::Id& receiver_router_id = getRouterIdFromInputEndpoint(buffer_msg->_input_endpoint);

         addNetPacketEndpoints(new_net_packet, getRouterId(), receiver_router_id);
         net_packet_list.push_back(new_net_packet);
      }

      break;

   default:
      LOG_PRINT_ERROR("Unsupported Msg Type (%u)", network_msg->_type);
      break;

   }
   LOG_PRINT("constructNetPackets(%p) end", network_msg);
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
   LOG_PRINT("performRouterAndLinkTraversal(%p) start", output_network_msg)
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

         // Incremenet Zero Load Delay
         flit->_zero_load_delay += (_router_performance_model->getDataPipelineDelay() +
                                    _link_performance_model_list[output_channel]->getDelay());
      }
      
      break;

   case NetworkMsg::BUFFER_MANAGEMENT:
      
      {
         BufferManagementMsg* buffer_msg = (BufferManagementMsg*) output_network_msg;
         SInt32 output_channel = buffer_msg->_output_endpoint._channel_id;

         // Link Performance Model - Is there a separate channel for buffer management msgs ?
         if (_link_performance_model_list[output_channel])
            buffer_msg->_normalized_time += _link_performance_model_list[output_channel]->getDelay();
      
         // Add Credit Pipeline Delay
         buffer_msg->_normalized_time += _router_performance_model->getCreditPipelineDelay();
         
         // FIXME: No power modeling for the buffer management messages           
      }
      
      break;

   default:
      LOG_PRINT_ERROR("Unrecognized NetworkMsg Type(%u)", output_network_msg->_type);
      break;
   }
   LOG_PRINT("performRouterAndLinkTraversal(%p) end", output_network_msg)
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
         }

         break;

      case NetworkMsg::BUFFER_MANAGEMENT:
         
         {            
            BufferManagementMsg* buffer_msg = (BufferManagementMsg*) network_msg;
            // Re-normalize according to average rate of progress
            UInt64 renormalized_time = buffer_msg->_normalized_time;
            // _time_normalizer->renormalize(buffer_msg->_normalized_time, buffer_msg->_average_rate_of_progress);
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

void
NetworkNode::printNetPacket(NetPacket* net_packet, bool is_input_msg)
{
   NetworkMsg* network_msg = (NetworkMsg*) net_packet->data;

   LOG_PRINT("Network Msg [Type(%s), Time(%llu), Normalized Time(%llu), Sender Router(%i,%i), Receiver Router(%i,%i), Input Endpoint(%i,%i), Output Endpoint(%i,%i)]",
         network_msg->getTypeString().c_str(),
         (long long unsigned int) net_packet->time,
         (long long unsigned int) network_msg->_normalized_time,
         net_packet->sender, network_msg->_sender_router_index,
         net_packet->receiver, network_msg->_receiver_router_index,
         network_msg->_input_endpoint._channel_id, network_msg->_input_endpoint._index,
         network_msg->_output_endpoint._channel_id, network_msg->_output_endpoint._index);

   switch (network_msg->_type)
   {

   case NetworkMsg::DATA:
      
      {
         Flit* flit = (Flit*) network_msg;
         LOG_PRINT("Flit [Type(%s), Normalized Time at Entry(%llu), Delay(%llu), Length(%i), Sender(%i), Receiver(%i), Requester(%i), Net Packet(%p), Output Endpoint List(%p)]",
               flit->getTypeString().c_str(),
               (long long unsigned int) flit->_normalized_time_at_entry,
               (long long unsigned int) (flit->_normalized_time - flit->_normalized_time_at_entry),
               flit->_length,
               flit->_sender,
               flit->_receiver,
               flit->_requester,
               flit->_net_packet,
               flit->_output_endpoint_list);

         if ((flit->_type == Flit::HEAD) && (is_input_msg))
         {
            ChannelEndpointList* endpoint_list = flit->_output_endpoint_list;

            ostringstream endpoints_str;
            endpoints_str << "Head Flit [Output Endpoint List( ";
            
            while (endpoint_list->curr() != endpoint_list->last())
            {
               endpoints_str << "(" << endpoint_list->curr()._channel_id << "," << endpoint_list->curr()._index << "), ";
               endpoint_list->incr();
            }
            endpoints_str <<  "(" << endpoint_list->curr()._channel_id << "," << endpoint_list->curr()._index << ") )]";
            endpoint_list->incr();

            LOG_PRINT("%s", endpoints_str.str().c_str());
         }
      }

      break;

   case NetworkMsg::BUFFER_MANAGEMENT:
            
      {
         BufferManagementMsg* buffer_msg = (BufferManagementMsg*) network_msg;
         LOG_PRINT("Buffer Management Msg [Type(%s)]", buffer_msg->getTypeString().c_str());

         if (buffer_msg->_type == BufferManagementScheme::CREDIT)
         {
            CreditMsg* credit_msg = (CreditMsg*) buffer_msg;
            LOG_PRINT("Credit Msg [Num Credits(%i)]", credit_msg->_num_credits);            
         }
         else if (buffer_msg->_type == BufferManagementScheme::ON_OFF)
         {
            OnOffMsg* on_off_msg = (OnOffMsg*) buffer_msg;
            LOG_PRINT("On Off Msg [Status(%s)]", on_off_msg->_on_off_status ? "TRUE" : "FALSE");
         }
      }

      break;

   default:
      LOG_PRINT_ERROR("Unrecognized Network Msg Type(%u)", network_msg->_type);
      break;
   }
}
