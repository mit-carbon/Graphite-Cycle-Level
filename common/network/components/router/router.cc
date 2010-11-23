#include "router.h"
#include "log.h"

Router::Router(Id id, \
      UInt32 flit_width, \
      RouterPerformanceModel* router_performance_model, \
      RouterPowerModel* router_power_model, \
      vector<LinkPerformanceModel*>& link_performance_model_list, \
      vector<LinkPowerModel*>& link_power_model_list, \
      vector<vector<Router::Id> >& input_channel_to_router_id_list__mapping, \
      vector<vector<Router::Id> >& output_channel_to_router_id_list__mapping):
   _id(id),
   _flit_width(flit_width),
   _router_performance_model(router_performance_model),
   _router_power_model(router_power_model),
   _link_performance_model_list(link_performance_model_list),
   _link_power_model_list(link_power_model_list),
   _input_channel_to_router_id_list__mapping(input_channel_to_router_id_list__mapping),
   _output_channel_to_router_id_list__mapping(output_channel_to_router_id_list__mapping)
{
   createMappings();
}

Router::~Router()
{}

void
Router::processNetworkMsg(NetworkMsg* network_msg, vector<NetworkMsg*>& network_msg_list_to_send)
{
   LOG_PRINT("processNetworkMsg(%p, %p) enter", this, network_msg);

   switch (network_msg->_type)
   {
      case NetworkMsg::DATA:
         
         {
            Flit* flit = (Flit*) network_msg;
            NetPacket* net_packet = flit->_net_packet;
            assert(net_packet);
            
            flit->_normalized_time = normalizeTime(net_packet->time);
            // Set the entry time to account for the time spent in the router
            flit->_normalized_time_at_entry = flit->_normalized_time;

            _router_performance_model->processDataMsg(flit, network_msg_list_to_send);
         }

         break;

      case NetworkMsg::BUFFER_MANAGEMENT:

         {
            BufferManagementMsg* buffer_msg = (BufferManagementMsg*) network_msg;
            // Add Credit Pipeline Delay
            buffer_msg->_normalized_time += _router_performance_model->getCreditPipelineDelay();
            _router_performance_model->processBufferManagementMsg(buffer_msg, network_msg_list_to_send);
         }

         break;

      default:
         LOG_PRINT_ERROR("Unrecognized network msg type(%u)", network_msg->_type);
         break;
   }

   vector<NetworkMsg*>::iterator msg_it = network_msg_list_to_send.begin();
   for ( ; msg_it != network_msg_list_to_send.end(); msg_it ++)
   {
      NetworkMsg* network_msg_to_send = *msg_it;
      // Account for router and link delays + update dynamic energy
      performRouterAndLinkTraversal(network_msg_to_send);
   }
   
   LOG_PRINT("processNetworkMsg(%p, %p) exit", this, network_msg);
}

void
Router::performRouterAndLinkTraversal(NetworkMsg* network_msg_to_send)
{
   switch (network_msg_to_send->_type)
   {
      case NetworkMsg::DATA:
         
         {
            Flit* flit_to_send = (Flit*) network_msg_to_send;
            SInt32 output_channel = flit_to_send->_output_endpoint._channel_id;

            // Router Performance Model (Data Pipeline delay)
            flit_to_send->_normalized_time += _router_performance_model->getDataPipelineDelay();
            // Router Power Model
            _router_power_model->updateDynamicEnergy(_flit_width/2, flit_to_send->_length);
            // Link Performance Model (Link delay)
            flit_to_send->_normalized_time += _link_performance_model_list[output_channel]->getDelay();
            // Link Power Model
            _link_power_model_list[output_channel]->updateDynamicEnergy(_flit_width/2, flit_to_send->_length);
         }
         
         break;

      case NetworkMsg::BUFFER_MANAGEMENT:
         
         {
            BufferManagementMsg* buffer_msg_to_send = (BufferManagementMsg*) network_msg_to_send;
            SInt32 input_channel = buffer_msg_to_send->_input_endpoint._channel_id;

            // FIXME: No power modeling for the buffer management messages
            // Link Performance Model
            buffer_msg_to_send->_normalized_time += _link_performance_model_list[input_channel]->getDelay();
         }
         
         break;

      default:
         LOG_PRINT_ERROR("Unrecognized NetworkMsg Type(%u)", network_msg_to_send->_type);
         break;
   }
}

void
Router::addChannelMapping(vector<vector<Router::Id> >& channel_to_router_id_list__mapping, \
      Router::Id& router_id)
{
   vector<Router::Id> router_id_list(1, router_id);
   channel_to_router_id_list__mapping.push_back(router_id_list);
}

void
Router::addChannelMapping(vector<vector<Router::Id> >& channel_to_router_id_list__mapping, \
      vector<Router::Id>& router_id_list)
{
   channel_to_router_id_list__mapping.push_back(router_id_list);
}

void
Router::createMappings()
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
Router::getInputEndpointFromRouterId(Router::Id& router_id)
{
   return _router_id_to_input_endpoint_mapping[router_id];
}

Channel::Endpoint&
Router::getOutputEndpointFromRouterId(Router::Id& router_id)
{
   return _router_id_to_output_endpoint_mapping[router_id];
}

Router::Id&
Router::getRouterIdFromInputEndpoint(Channel::Endpoint& input_endpoint)
{
   assert(input_endpoint._index != Channel::Endpoint::ALL);
   return _input_channel_to_router_id_list__mapping[input_endpoint._channel_id][input_endpoint._index];
}

Router::Id&
Router::getRouterIdFromOutputEndpoint(Channel::Endpoint& output_endpoint)
{
   return _output_channel_to_router_id_list__mapping[output_endpoint._channel_id][output_endpoint._index];
}

vector<Router::Id>&
Router::getRouterIdListFromOutputChannel(SInt32 output_channel_id)
{
   return _output_channel_to_router_id_list__mapping[output_channel_id];
}
