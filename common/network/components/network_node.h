#pragma once

#include <vector>
using namespace std;

#include "fixed_types.h"
#include "network.h"
#include "network_msg.h"
#include "buffer_management_scheme.h"
#include "router_performance_model.h"
#include "router_power_model.h"
#include "link_performance_model.h"
#include "link_power_model.h"
#include "channel.h"
#include "router.h"
#include "time_normalizer.h"

class NetworkNode
{
public:
   NetworkNode(Router::Id router_id, \
         UInt32 flit_width, \
         RouterPerformanceModel* router_performance_model, \
         RouterPowerModel* router_power_model, \
         vector<LinkPerformanceModel*>& link_performance_model_list, \
         vector<LinkPowerModel*>& link_power_model_list, \
         vector<vector<Router::Id> >& input_channel_to_router_id_list__mapping, \
         vector<vector<Router::Id> >& output_channel_to_router_id_list__mapping);
   ~NetworkNode();

   Router::Id getRouterId() { return _router_id; }

   static void addChannelMapping(vector<vector<Router::Id> >& channel_to_router_id_list__mapping, \
         Router::Id& router_id);
   static void addChannelMapping(vector<vector<Router::Id> >& channel_to_router_id_list__mapping, \
         vector<Router::Id>& router_id);

   // Process NetworkMsg
   void processNetPacket(NetPacket* input_net_packet, list<NetPacket*>& output_net_packet_list);

   // Channel::Endpoint <--> Router::Id
   Channel::Endpoint& getInputEndpointFromRouterId(Router::Id& router_id);
   Channel::Endpoint& getOutputEndpointFromRouterId(Router::Id& router_id);
   Router::Id& getRouterIdFromInputEndpoint(Channel::Endpoint& input_endpoint);
   Router::Id& getRouterIdFromOutputEndpoint(Channel::Endpoint& output_endpoint);
   vector<Router::Id>& getRouterIdListFromOutputChannel(SInt32 output_channel_id);

   // RouterPerformanceModel
   RouterPerformanceModel* getRouterPerformanceModel()
   { return _router_performance_model; }

   // Time Normalizer
   TimeNormalizer* getTimeNormalizer()
   { return _time_normalizer; }

private:
   Router::Id _router_id;

   // Flit Width
   UInt32 _flit_width;

   // Router & Link Performance & Power Models
   RouterPerformanceModel* _router_performance_model;
   RouterPowerModel* _router_power_model;
   vector<LinkPerformanceModel*> _link_performance_model_list;
   vector<LinkPowerModel*> _link_power_model_list;

   // Time Normalizer
   TimeNormalizer* _time_normalizer;

   // Endpoint <-> Router Id mapping
   vector<vector<Router::Id> > _input_channel_to_router_id_list__mapping;
   vector<vector<Router::Id> > _output_channel_to_router_id_list__mapping;
   map<Router::Id, Channel::Endpoint> _router_id_to_input_endpoint_mapping;
   map<Router::Id, Channel::Endpoint> _router_id_to_output_endpoint_mapping;

   // Error Checking
   UInt64 _last_net_packet_time;

   // Normalize Time
   void normalizeTime(NetworkMsg* network_msg);
   // Communicate Progress Rate Info to adjoining router
   void communicateProgressRateInfo(BufferManagementMsg* buffer_msg);
   // Perform Router and Link Traversal
   void performRouterAndLinkTraversal(NetworkMsg* output_network_msg);
   // Construct NetPackets
   void constructNetPackets(NetworkMsg* network_msg, list<NetPacket*>& net_packet_list);
   // Add NetPacket Endpoints
   void addNetPacketEndpoints(NetPacket* net_packet, Router::Id sender_router_id, Router::Id receiver_router_id);
  
   // Create Router <-> Channel Mappings 
   void createMappings();

   // Print NetPacket
   void printNetPacket(NetPacket* net_packet, bool is_input_msg = false);
};
