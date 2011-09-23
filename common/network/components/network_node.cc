#include <sstream>
using std::ostringstream;
#include "simulator.h"
#include "core_manager.h"
#include "core.h"
#include "finite_buffer_network_model.h"
#include "network_node.h"
#include "utils.h"
#include "credit_msg.h"
#include "on_off_msg.h"
#include "log.h"

NetworkNode::NetworkNode(Router::Id router_id,
      UInt32 flit_width,
      RouterPerformanceModel* router_performance_model,
      RouterPowerModel* router_power_model,
      vector<LinkPerformanceModel*> link_performance_model_list,
      vector<LinkPowerModel*> link_power_model_list,
      vector<vector<Router::Id> > input_channel_to_router_id_list__mapping,
      vector<vector<Router::Id> > output_channel_to_router_id_list__mapping,
      PacketType flow_control_packet_type):
   _router_id(router_id),
   _flit_width(flit_width),
   _router_performance_model(router_performance_model),
   _router_power_model(router_power_model),
   _link_performance_model_list(link_performance_model_list),
   _link_power_model_list(link_power_model_list),
   _input_channel_to_router_id_list__mapping(input_channel_to_router_id_list__mapping),
   _output_channel_to_router_id_list__mapping(output_channel_to_router_id_list__mapping),
   _flow_control_packet_type(flow_control_packet_type),
   _last_net_packet_time(0)
{
   assert(_router_performance_model);
   createMappings();

   // Number of Input and Output Channels
   _num_input_channels = _input_channel_to_router_id_list__mapping.size();
   _num_output_channels = _output_channel_to_router_id_list__mapping.size();
  
   // Initialize Event Counters
   initializeEventCounters();
   // Initialize Contention Model Counters
   initializeContentionModelCounters();

   // printNode();
}

NetworkNode::~NetworkNode()
{}

void
NetworkNode::processNetPacket(NetPacket* input_net_packet, list<NetPacket*>& output_net_packet_list)
{
   LOG_ASSERT_ERROR(input_net_packet->time >= _last_net_packet_time,
         "Curr Net Packet Time(%llu), Last Net Packet Time(%llu)", input_net_packet->time, _last_net_packet_time);
   _last_net_packet_time = input_net_packet->time;

   NetworkMsg* input_network_msg = (NetworkMsg*) input_net_packet->data;
   vector<NetworkMsg*> output_network_msg_list;

   Router::Id sender_router_id(input_net_packet->sender, input_network_msg->_sender_router_index);
   Router::Id receiver_router_id(input_net_packet->receiver, input_network_msg->_receiver_router_index);
   LOG_ASSERT_ERROR(receiver_router_id == _router_id,
         "Receiver Router ID(%i,%i), Curr Router ID(%i,%i)",
         receiver_router_id._core_id, receiver_router_id._index, _router_id._core_id, _router_id._index);
   
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

         // Update Event Counters
         updateEventCounters(flit);

         printNetPacket(input_net_packet, true);
        
         LOG_PRINT("processDataMsg: Flit[Type(%s),Time(%llu),Input Endpoint(%i,%i),Sender Router(%i,%i),Receiver Router(%i,%i)]",
               flit->getTypeString().c_str(), flit->_normalized_time, flit->_input_endpoint._channel_id, flit->_input_endpoint._index,
               input_net_packet->sender, flit->_sender_router_index, _router_id._core_id, _router_id._index);
         _router_performance_model->processDataMsg(flit, output_network_msg_list);
         LOG_PRINT("processDataMsg: end - output msg list size(%u)", output_network_msg_list.size());
      }

      break;

   case NetworkMsg::BUFFER_MANAGEMENT:

      {
         BufferManagementMsg* buffer_msg = (BufferManagementMsg*) input_network_msg;
            
         // output_endpoint (for buffer management msgs)
         buffer_msg->_output_endpoint = getOutputEndpointFromRouterId(sender_router_id);

         // Normalize Time 
         normalizeTime(buffer_msg);
         
         printNetPacket(input_net_packet, true);

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
      
      // Account for router(credit/data pipeline) and link delays + update dynamic energy
      performRouterAndLinkTraversal(output_network_msg);

      // Construct NetPacket and add to list
      constructNetPackets(output_network_msg, output_net_packet_list);
   }

   for (list<NetPacket*>::iterator it = output_net_packet_list.begin();
         it != output_net_packet_list.end(); it ++)
   {
      printNetPacket(*it);
   }
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

         // Create new net_packet struct
         NetPacket* new_net_packet = new NetPacket(buffer_msg->_normalized_time /* time */,
               _flow_control_packet_type,
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

         // Update Contention Model Counters - when zero load delay is not factored in
         updateContentionCounters(flit);
         
         SInt32 output_channel = flit->_output_endpoint._channel_id;

         // Router Performance Model (Data Pipeline delay)
         flit->_normalized_time += _router_performance_model->getDataPipelineDelay();
         
         // Router Power Model
         if (_router_power_model)
            _router_power_model->updateDynamicEnergy(_flit_width/2, flit->_num_phits);
         
         // Link Performance Model (Link delay)
         UInt64 link_delay = 0;
         if (_link_performance_model_list[output_channel])
            link_delay = _link_performance_model_list[output_channel]->getDelay();
         flit->_normalized_time += link_delay;
         
         // Link Power Model
         if (_link_power_model_list[output_channel])
            _link_power_model_list[output_channel]->updateDynamicEnergy(_flit_width/2, flit->_num_phits);

         // Increment Zero Load Delay
         flit->_zero_load_delay += (_router_performance_model->getDataPipelineDelay() + link_delay);
      }
      
      break;

   case NetworkMsg::BUFFER_MANAGEMENT:
      
      {
         BufferManagementMsg* buffer_msg = (BufferManagementMsg*) output_network_msg;
         Channel::Endpoint input_endpoint = buffer_msg->_input_endpoint;

         NetworkNode* remote_network_node = getRemoteNetworkNode(_flow_control_packet_type, input_endpoint);
         assert(remote_network_node);
         // Link Performance Model - Is there a separate channel for buffer management msgs ?
         // Add Link Delay
         buffer_msg->_normalized_time += getRemoteLinkDelay(remote_network_node);
         // Add Credit Pipeline Delay
         buffer_msg->_normalized_time += getRemoteRouterCreditPipelineDelay(remote_network_node);

         // FIXME: No power modeling for the buffer management messages           
      }
      
      break;

   default:
      LOG_PRINT_ERROR("Unrecognized NetworkMsg Type(%u)", output_network_msg->_type);
      break;
   }
}

NetworkNode*
NetworkNode::getRemoteNetworkNode(PacketType packet_type, Channel::Endpoint& input_endpoint)
{
   Router::Id remote_router_id = getRouterIdFromInputEndpoint(input_endpoint);
   Core* remote_core = Sim()->getCoreManager()->getCoreFromID(remote_router_id._core_id);
   FiniteBufferNetworkModel* network_model = (FiniteBufferNetworkModel*) (remote_core->getNetwork()->getNetworkModelFromPacketType(packet_type));
   return network_model->getNetworkNode(remote_router_id._index);
}

UInt64
NetworkNode::getRemoteRouterDataPipelineDelay(NetworkNode* remote_network_node)
{
   return remote_network_node->getRouterPerformanceModel()->getDataPipelineDelay();
}

UInt64
NetworkNode::getRemoteRouterCreditPipelineDelay(NetworkNode* remote_network_node)
{
   return remote_network_node->getRouterPerformanceModel()->getCreditPipelineDelay();
}

UInt64
NetworkNode::getRemoteLinkDelay(NetworkNode* remote_network_node)
{
   // Link that connects remote_network_node and curr_network_node
   Channel::Endpoint output_endpoint = remote_network_node->getOutputEndpointFromRouterId(_router_id);
   SInt32 output_channel = output_endpoint._channel_id;
   LinkPerformanceModel* link_performance_model = remote_network_node->getLinkPerformanceModel(output_channel);
   return (link_performance_model ? link_performance_model->getDelay() : 0);
}

void
NetworkNode::initializeEventCounters()
{
   _total_input_buffer_writes = 0;
   _total_input_buffer_reads = 0;
   _total_switch_allocator_requests = 0;
   _total_crossbar_traversals.resize(_num_output_channels, 0);
   _total_output_link_unicasts.resize(_num_output_channels, 0);
   _total_output_link_broadcasts.resize(_num_output_channels, 0);

   _cached_output_endpoint_list.resize(_num_input_channels, 0);
}

void
NetworkNode::initializeContentionModelCounters()
{
   _total_contention_delay_counters.resize(_num_input_channels, 0);
   _total_flits_processed.resize(_num_input_channels, 0);
}

void
NetworkNode::updateEventCounters(Flit* flit)
{
   SInt32 input_channel = flit->_input_endpoint._channel_id;
   vector<Channel::Endpoint>* output_endpoint_list;
   if (flit->_type & Flit::HEAD)
   {
      HeadFlit* head_flit = (HeadFlit*) flit;
      output_endpoint_list = head_flit->_output_endpoint_list;
      setOutputEndpointList(input_channel, output_endpoint_list);
   }
   else // (flit->_type != Flit::HEAD)
   {
      output_endpoint_list = getOutputEndpointList(input_channel);
   }

   if (flit->_type & Flit::TAIL)
      setOutputEndpointList(input_channel, NULL);
   
   // Increment Event Counters
   
   // Input Buffer Writes
   _total_input_buffer_writes += flit->_num_phits;
   // Input Buffer Reads
   _total_input_buffer_reads += flit->_num_phits;
   
   // Switch Allocator Requests
   _total_switch_allocator_requests += ((flit->_type & Flit::HEAD) ? 1 : 0);
   
   // Crossbar Traversals - depending on multicast index
   if (output_endpoint_list->size() > 0)
      _total_crossbar_traversals[output_endpoint_list->size() - 1] += flit->_num_phits;

   // Link Traversals - based on number of endpoints to which it is forwarded 
   vector<Channel::Endpoint>::iterator it = output_endpoint_list->begin();
   for ( ; it != output_endpoint_list->end(); it++)
   {
      Channel::Endpoint endpoint = (*it);
      if (endpoint._index == Channel::Endpoint::ALL)
         _total_output_link_broadcasts[endpoint._channel_id] += flit->_num_phits;
      else // (endpoint._index != Channel::Endpoint::ALL)
         _total_output_link_unicasts[endpoint._channel_id] += flit->_num_phits;
   }
}

void
NetworkNode::updateContentionCounters(Flit* flit)
{
   SInt32 input_channel = flit->_input_endpoint._channel_id;
   if (_router_id._index == FiniteBufferNetworkModel::NET_PACKET_INJECTOR)
      _total_contention_delay_counters[input_channel] += (flit->_normalized_time - flit->_net_packet->start_time);
   else // (_router_id._index != FiniteBufferNetworkModel::NET_PACKET_INJECTOR)
      _total_contention_delay_counters[input_channel] += (flit->_normalized_time - flit->_normalized_time_at_entry);
   _total_flits_processed[input_channel] ++;
}

UInt64
NetworkNode::getTotalInputBufferWrites()
{
   return _total_input_buffer_writes;
}

UInt64
NetworkNode::getTotalInputBufferReads()
{
   return _total_input_buffer_reads;
}

UInt64
NetworkNode::getTotalSwitchAllocatorRequests()
{
   return _total_switch_allocator_requests;
}

UInt64
NetworkNode::getTotalCrossbarTraversals(SInt32 multicast_index)
{
   assert(multicast_index >= 1 && multicast_index <= _num_output_channels);
   return _total_crossbar_traversals[multicast_index-1];
}

UInt64
NetworkNode::getTotalOutputLinkUnicasts(SInt32 link_id_start, SInt32 link_id_end)
{
   if (link_id_start == Channel::ALL)
   {
      assert(link_id_end == Channel::INVALID);
      link_id_start = 0;
      link_id_end = _num_output_channels-1;
   }
   else if (link_id_end == Channel::INVALID)
   {
      link_id_end = link_id_start;
   }

   UInt64 total_link_unicasts = 0;
   for (SInt32 i = link_id_start; i <= link_id_end; i++)
   {
      total_link_unicasts += _total_output_link_unicasts[i];
   }
   return total_link_unicasts;
}

UInt64
NetworkNode::getTotalOutputLinkBroadcasts(SInt32 link_id_start, SInt32 link_id_end)
{
   if (link_id_start == Channel::ALL)
   {
      assert(link_id_end == Channel::INVALID);
      link_id_start = 0;
      link_id_end = _num_output_channels-1;
   }
   else if (link_id_end == Channel::INVALID)
   {
      link_id_end = link_id_start;
   }

   UInt64 total_link_broadcasts = 0;
   for (SInt32 i = link_id_start; i <= link_id_end; i++)
   {
      total_link_broadcasts += _total_output_link_broadcasts[i];
   }
   return total_link_broadcasts;
}

double
NetworkNode::getAverageContentionDelay()
{
   UInt64 total_contention_delay = 0;
   UInt64 total_flits_processed = 0;
   for (SInt32 i = 0; i < _num_input_channels; i++)
   {
      total_contention_delay += _total_contention_delay_counters[i];
      total_flits_processed += _total_flits_processed[i];
   }
   return ((double) total_contention_delay) / total_flits_processed;
}

vector<Channel::Endpoint>*
NetworkNode::getOutputEndpointList(SInt32 input_channel)
{
   assert(_cached_output_endpoint_list[input_channel]);
   return _cached_output_endpoint_list[input_channel];
}

void
NetworkNode::setOutputEndpointList(SInt32 input_channel, vector<Channel::Endpoint>* output_endpoint_list)
{
   assert( (output_endpoint_list == NULL) == (_cached_output_endpoint_list[input_channel] != NULL) );
   _cached_output_endpoint_list[input_channel] = output_endpoint_list;
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
      break;

   default:
      LOG_PRINT_ERROR("Unrecognized NetworkMsg type(%u)", network_msg->_type);
      break;
   }
}

void
NetworkNode::addChannelMapping(vector<vector<Router::Id> >& channel_to_router_id_list__mapping,
      Router::Id& router_id)
{
   vector<Router::Id> router_id_list(1, router_id);
   channel_to_router_id_list__mapping.push_back(router_id_list);
}

void
NetworkNode::addChannelMapping(vector<vector<Router::Id> >& channel_to_router_id_list__mapping,
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
         _router_id_to_input_endpoint_mapping.insert(
               make_pair<Router::Id, Channel::Endpoint>(
                  Router::Id(_input_channel_to_router_id_list__mapping[i][j]),
                  Channel::Endpoint(i,j)
               )
            );
      }
   }

   for (SInt32 i = 0; i < (SInt32) _output_channel_to_router_id_list__mapping.size(); i++)
   {
      for (SInt32 j = 0; j < (SInt32) _output_channel_to_router_id_list__mapping[i].size(); j++)
      {
         _router_id_to_output_endpoint_mapping.insert(
               make_pair<Router::Id, Channel::Endpoint>(
                  Router::Id(_output_channel_to_router_id_list__mapping[i][j]),
                  Channel::Endpoint(i,j)
               )
            );
      }
   }
}

Channel::Endpoint&
NetworkNode::getInputEndpointFromRouterId(Router::Id& router_id)
{
   LOG_ASSERT_ERROR(_router_id_to_input_endpoint_mapping.find(router_id) != _router_id_to_input_endpoint_mapping.end(),
         "Router ID(%i,%i), Network Node(%i,%i)", router_id._core_id, router_id._index, _router_id._core_id, _router_id._index);
   return _router_id_to_input_endpoint_mapping[router_id];
}

Channel::Endpoint&
NetworkNode::getOutputEndpointFromRouterId(Router::Id& router_id)
{
   LOG_ASSERT_ERROR(_router_id_to_output_endpoint_mapping.find(router_id) != _router_id_to_output_endpoint_mapping.end(),
         "Router ID(%i,%i), Network Node(%i,%i)", router_id._core_id, router_id._index, _router_id._core_id, _router_id._index);
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
NetworkNode::printNode()
{
   fprintf(stderr, "\n\nRouter Id(%i,%i)\n", _router_id._core_id, _router_id._index);
   fprintf(stderr, "Input Channels\n");
   for (UInt32 i = 0; i < _input_channel_to_router_id_list__mapping.size(); i++)
   {
      fprintf(stderr, "%i -> ", i);
      for (UInt32 j = 0; j < _input_channel_to_router_id_list__mapping[i].size(); j++)
      {
         Router::Id router_id = _input_channel_to_router_id_list__mapping[i][j];
         fprintf(stderr, "(%i,%i), ", router_id._core_id, router_id._index);
      }
      fprintf(stderr, "\n");
   }
   fprintf(stderr, "Output Channels\n");
   for (UInt32 i = 0; i < _output_channel_to_router_id_list__mapping.size(); i++)
   {
      fprintf(stderr, "%i -> ", i);
      for (UInt32 j = 0; j < _output_channel_to_router_id_list__mapping[i].size(); j++)
      {
         Router::Id router_id = _output_channel_to_router_id_list__mapping[i][j];
         fprintf(stderr, "(%i,%i), ", router_id._core_id, router_id._index);
      }
      fprintf(stderr, "\n");
   }
}

void
NetworkNode::printNetPacket(NetPacket* net_packet, bool is_input_msg)
{
   NetworkMsg* network_msg = (NetworkMsg*) net_packet->data;

   char output_str[1000] = "";
   sstrcat(output_str, "%s: Type(%s), Time(%lu), Upstream(%i,%i), Downstream(%i,%i)",
                       is_input_msg ? "INPUT" : "OUTPUT",
                       network_msg->getTypeString().c_str(),
                       network_msg->_normalized_time,
                       net_packet->sender, network_msg->_sender_router_index,
                       net_packet->receiver, network_msg->_receiver_router_index);

   switch (network_msg->_type)
   {

   case NetworkMsg::DATA:
      
      {
         Flit* flit = (Flit*) network_msg;

         sstrcat(output_str, ", sender(%i), receiver(%i)", flit->_sender, flit->_receiver);

         if ((flit->_type & Flit::HEAD) && (is_input_msg))
         {
            HeadFlit* head_flit = (HeadFlit*) flit;
            vector<Channel::Endpoint>* endpoint_list = head_flit->_output_endpoint_list;

            ostringstream endpoints_str;
            sstrcat(output_str, ", HEAD-Flit[");
           
            if (!endpoint_list->empty())
            {
               vector<Channel::Endpoint>::iterator endpoint_it = endpoint_list->begin(); 
               for ( ; endpoint_it != endpoint_list->end(); endpoint_it ++)
               {
                  Channel::Endpoint endpoint = *endpoint_it;
                  sstrcat(output_str, "(%i,%i),", endpoint._channel_id, endpoint._index);
               }
            }

            sstrcat(output_str, "]");
         }

         if (!is_input_msg)
         {
            sstrcat(output_str, ", Delay(%lu)", flit->_normalized_time - flit->_normalized_time_at_entry);
         }
      }

      break;

   case NetworkMsg::BUFFER_MANAGEMENT:
            
      {
         BufferManagementMsg* buffer_msg = (BufferManagementMsg*) network_msg;

         if (buffer_msg->_type == BufferManagementScheme::CREDIT)
         {
            CreditMsg* credit_msg = (CreditMsg*) buffer_msg;
            sstrcat(output_str, ", Credit(%i)", credit_msg->_num_credits);
         }
         else if (buffer_msg->_type == BufferManagementScheme::ON_OFF)
         {
            OnOffMsg* on_off_msg = (OnOffMsg*) buffer_msg;
            sstrcat(output_str, ", On-Off(%s)", on_off_msg->_on_off_status ? "true" : "false");
         }
      }

      break;

   default:
      LOG_PRINT_ERROR("Unrecognized Network Msg Type(%u)", network_msg->_type);
      break;
   }

   LOG_PRINT("%s", output_str);
}
