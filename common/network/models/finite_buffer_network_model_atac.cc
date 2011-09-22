#include <cmath>
#include <algorithm>
using namespace std;

#include "router.h"
#include "router_performance_model.h"
#include "router_power_model.h"
#include "electrical_link_performance_model.h"
#include "electrical_link_power_model.h"
#include "optical_link_performance_model.h"
#include "optical_link_power_model.h"
#include "finite_buffer_network_model_atac.h"
#include "simulator.h"
#include "config.h"
#include "utils.h"
#include "log.h"

//// Static Variables
// Is Initialized?
bool FiniteBufferNetworkModelAtac::_initialized = false;
// ENet
SInt32 FiniteBufferNetworkModelAtac::_enet_width;
SInt32 FiniteBufferNetworkModelAtac::_enet_height;
// Clusters
SInt32 FiniteBufferNetworkModelAtac::_num_clusters;
SInt32 FiniteBufferNetworkModelAtac::_cluster_size;
SInt32 FiniteBufferNetworkModelAtac::_numX_clusters;
SInt32 FiniteBufferNetworkModelAtac::_numY_clusters;
SInt32 FiniteBufferNetworkModelAtac::_cluster_width;
SInt32 FiniteBufferNetworkModelAtac::_cluster_height;
// Sub Clusters
SInt32 FiniteBufferNetworkModelAtac::_num_optical_access_points;
SInt32 FiniteBufferNetworkModelAtac::_num_sub_clusters;
SInt32 FiniteBufferNetworkModelAtac::_numX_sub_clusters;
SInt32 FiniteBufferNetworkModelAtac::_numY_sub_clusters;
SInt32 FiniteBufferNetworkModelAtac::_sub_cluster_width;
SInt32 FiniteBufferNetworkModelAtac::_sub_cluster_height;
// Num BNets
SInt32 FiniteBufferNetworkModelAtac::_num_receive_nets_per_cluster;
// Cluster Boundaries and Access Points
vector<FiniteBufferNetworkModelAtac::ClusterInfo> FiniteBufferNetworkModelAtac::_cluster_info_list;
// Global Routing Strategy
FiniteBufferNetworkModelAtac::GlobalRoutingStrategy FiniteBufferNetworkModelAtac::_global_routing_strategy;
SInt32 FiniteBufferNetworkModelAtac::_unicast_distance_threshold;
// Type of Network on Receiver Cluster
FiniteBufferNetworkModelAtac::ReceiveNetType FiniteBufferNetworkModelAtac::_receive_net_type;

FiniteBufferNetworkModelAtac::FiniteBufferNetworkModelAtac(Network* network, SInt32 network_id):
   FiniteBufferNetworkModel(network, network_id)
{
   // Initialize ANet topology related parameters
   initializeANetTopologyParameters();
   initializeANetRoutingParameters();

   // Get the configuration parameters
   try
   {
      _frequency = Sim()->getCfg()->getFloat("network/atac/frequency");
      _flit_width = Sim()->getCfg()->getInt("network/atac/flit_width");
      _flow_control_scheme = FlowControlScheme::parse(
            Sim()->getCfg()->getString("network/atac/flow_control_scheme"));
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read ATAC model parameters from the config file");
   }

   // Cluster ID
   _cluster_id = computeClusterID(_core_id);

   // Create the routers
   _network_node_map[NET_PACKET_INJECTOR] = createNetPacketInjectorNode(Router::Id(_core_id, EMESH),
         BufferManagementScheme::parse(Sim()->getCfg()->getString("network/atac/buffer_management_scheme")),
         Sim()->getCfg()->getInt("network/atac/enet/router/input_buffer_size") );

   _network_node_map[EMESH] = createNetworkNode(EMESH);
   if (_core_id == computeCoreIDWithOpticalHub(_cluster_id))
   {
      _network_node_map[SEND_HUB] = createNetworkNode(SEND_HUB);
      _network_node_map[RECEIVE_HUB] = createNetworkNode(RECEIVE_HUB);

      if (_receive_net_type == STAR)
      {
         for (SInt32 i = 0; i < _num_receive_nets_per_cluster; i++)
         {
            SInt32 node_type = STAR_NET_ROUTER_BASE + i;
            _network_node_map[node_type] = createNetworkNode(node_type);
         }
      }
   }
}

FiniteBufferNetworkModelAtac::~FiniteBufferNetworkModelAtac()
{
   // Delete the router objects
   map<SInt32, NetworkNode*>::iterator it = _network_node_map.begin();
   for ( ; it != _network_node_map.end(); it ++)
      delete (*it).second;
}

FiniteBufferNetworkModelAtac::GlobalRoutingStrategy
FiniteBufferNetworkModelAtac::parseGlobalRoutingStrategy(string str)
{
   if (str == "distance_based")
      return DISTANCE_BASED;
   else if (str == "cluster_based")
      return CLUSTER_BASED;
   else
   {
      LOG_PRINT_ERROR("Unrecognized Global Routing Strategy(%s)", str.c_str());
      return (GlobalRoutingStrategy) -1;
   }
}

FiniteBufferNetworkModelAtac::ReceiveNetType
FiniteBufferNetworkModelAtac::parseReceiveNetType(string str)
{
   if (str == "htree")
      return HTREE;
   else if (str == "star")
      return STAR;
   else
   {
      LOG_PRINT_ERROR("Unrecognized Receive Net Type(%s)", str.c_str());
      return (ReceiveNetType) -1;
   }
}

FiniteBufferNetworkModelAtac::GlobalRoute
FiniteBufferNetworkModelAtac::computeGlobalRoute(core_id_t sender, core_id_t receiver)
{
   if (receiver == NetPacket::BROADCAST)
      return GLOBAL_ONET;

   // Unicast packet
   if (_global_routing_strategy == DISTANCE_BASED)
   {
      SInt32 distance = computeDistance(sender, receiver);
      if ( (distance <= _unicast_distance_threshold) || (computeClusterID(sender) == computeClusterID(receiver)) )
         return GLOBAL_ENET;
      else
         return GLOBAL_ONET;
   }
   else // CLUSTER_BASED global routing strategy
   {
      if (computeClusterID(sender) == computeClusterID(receiver))
         return GLOBAL_ENET;
      else
         return GLOBAL_ONET;
   }
}

SInt32
FiniteBufferNetworkModelAtac::computeReceiveNetID(core_id_t sender)
{
   SInt32 sending_cluster_id = computeClusterID(sender);
   return (sending_cluster_id % _num_receive_nets_per_cluster);
}


void
FiniteBufferNetworkModelAtac::initializeANetTopologyParameters()
{
   if (_initialized)
      return;
   _initialized = true;

   SInt32 total_cores = Config::getSingleton()->getTotalCores();

   try
   {
      _cluster_size = Sim()->getCfg()->getInt("network/atac/cluster_size");
      _num_optical_access_points = Sim()->getCfg()->getInt("network/atac/num_optical_access_points_per_cluster");
      _receive_net_type = parseReceiveNetType(Sim()->getCfg()->getString("network/atac/receive_net_type"));
      _num_receive_nets_per_cluster = Sim()->getCfg()->getInt("network/atac/num_receive_nets_per_cluster");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Error reading ATAC topology parameters from cfg file");
   }

   LOG_ASSERT_ERROR(isPerfectSquare(total_cores),
         "Total Cores(%i) must be a perfect square", total_cores);
   LOG_ASSERT_ERROR(isPower2(total_cores),
         "Total Cores(%i) must be a power of 2", total_cores);
   LOG_ASSERT_ERROR(isPower2(_cluster_size),
         "Cluster Size(%i) must be a power of 2", _cluster_size);
   LOG_ASSERT_ERROR((total_cores % _cluster_size) == 0,
         "Total Cores(%i) must be a multiple of Cluster Size(%i)", total_cores, _cluster_size);

   _num_clusters = total_cores / _cluster_size;
   LOG_ASSERT_ERROR(_num_clusters > 1, "Number of Clusters(%i) must be > 1", _num_clusters);
   
   _num_sub_clusters = _num_optical_access_points;
   LOG_ASSERT_ERROR(isPower2(_num_optical_access_points),
         "Number of Optical Access Points(%i) must be a power of 2", _num_optical_access_points);

   // Calculations with an electrical mesh
   _enet_width = (SInt32) sqrt(total_cores);
   _enet_height = _enet_width;

   initializeClusters();
}

void
FiniteBufferNetworkModelAtac::initializeANetRoutingParameters()
{
   try
   {
      // Global Routing Strategy
      _global_routing_strategy = parseGlobalRoutingStrategy(
            Sim()->getCfg()->getString("network/atac/global_routing_strategy"));
      // If Global Routing Strategy is distance based, the distance above which ONet should
      // be used for sending unicasts
      _unicast_distance_threshold = Sim()->getCfg()->getInt("network/atac/unicast_distance_threshold");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Error reading ATAC routing parameters from cfg file");
   }
}

NetworkNode*
FiniteBufferNetworkModelAtac::createNetworkNode(SInt32 node_type)
{
   // Router Parameters
   BufferManagementScheme::Type buffer_management_scheme;

   SInt32 enet_router_input_buffer_size = 0;
   SInt32 enet_router_data_pipeline_delay = 0;
   SInt32 enet_router_credit_pipeline_delay = 0;

   SInt32 send_hub_router_input_buffer_size = 0;
   SInt32 send_hub_router_data_pipeline_delay = 0;
   SInt32 send_hub_router_credit_pipeline_delay = 0;

   SInt32 receive_hub_router_input_buffer_size = 0;
   SInt32 receive_hub_router_data_pipeline_delay = 0;
   SInt32 receive_hub_router_credit_pipeline_delay = 0;

   SInt32 star_net_router_input_buffer_size = 0;
   SInt32 star_net_router_data_pipeline_delay = 0;
   SInt32 star_net_router_credit_pipeline_delay = 0;
  
   // Link Parameters 
   string electrical_link_type;

   // Other parameters 
   SInt32 router_input_buffer_size = 0;
   SInt32 router_data_pipeline_delay = 0;
   SInt32 router_credit_pipeline_delay = 0;

   try
   {
      // Buffer Management Scheme
      buffer_management_scheme = BufferManagementScheme::parse(
            Sim()->getCfg()->getString("network/atac/buffer_management_scheme"));

      // ENet Router
      enet_router_input_buffer_size = Sim()->getCfg()->getInt(
            "network/atac/enet/router/input_buffer_size");
      enet_router_data_pipeline_delay = Sim()->getCfg()->getInt(
            "network/atac/enet/router/data_pipeline_delay");
      enet_router_credit_pipeline_delay = Sim()->getCfg()->getInt(
            "network/atac/enet/router/credit_pipeline_delay");
      
      // Send Hub Router
      send_hub_router_input_buffer_size = Sim()->getCfg()->getInt(
            "network/atac/onet/send_hub/router/input_buffer_size");
      send_hub_router_data_pipeline_delay = Sim()->getCfg()->getInt(
            "network/atac/onet/send_hub/router/data_pipeline_delay");
      send_hub_router_credit_pipeline_delay = Sim()->getCfg()->getInt(
            "network/atac/onet/send_hub/router/credit_pipeline_delay");

      // Receive Hub Router
      receive_hub_router_input_buffer_size = Sim()->getCfg()->getInt(
            "network/atac/onet/receive_hub/router/input_buffer_size");
      receive_hub_router_data_pipeline_delay = Sim()->getCfg()->getInt(
            "network/atac/onet/receive_hub/router/data_pipeline_delay");
      receive_hub_router_credit_pipeline_delay = Sim()->getCfg()->getInt(
            "network/atac/onet/receive_hub/router/credit_pipeline_delay");

      // Star Net Router
      star_net_router_input_buffer_size = Sim()->getCfg()->getInt(
            "network/atac/star_net/router/input_buffer_size");
      star_net_router_data_pipeline_delay = Sim()->getCfg()->getInt(
            "network/atac/star_net/router/data_pipeline_delay");
      star_net_router_credit_pipeline_delay = Sim()->getCfg()->getInt(
            "network/atac/star_net/router/credit_pipeline_delay");

      electrical_link_type = Sim()->getCfg()->getString("network/atac/electrical_link_type");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read ATAC network parameters from the cfg file");
   }

   vector<vector<Router::Id> > input_channel_to_router_id_list__mapping;
   vector<SInt32> num_input_endpoints_list;
   vector<BufferManagementScheme::Type> input_buffer_management_schemes;
   vector<SInt32> input_buffer_size_list;

   vector<vector<Router::Id> > output_channel_to_router_id_list__mapping;
   vector<SInt32> num_output_endpoints_list;
   vector<BufferManagementScheme::Type> downstream_buffer_management_schemes;
   vector<SInt32> downstream_buffer_size_list;

   // Core ID list in cluster
   vector<core_id_t> core_id_list_in_cluster;
   computeCoreIDListInCluster(_cluster_id, core_id_list_in_cluster);
   assert(_cluster_size == (SInt32) core_id_list_in_cluster.size());

   if (node_type == EMESH)
   {
      // Add the core interface
      Router::Id core_interface(_core_id, CORE_INTERFACE);
      Router::Id net_packet_injector_node(_core_id, NET_PACKET_INJECTOR);

      NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, net_packet_injector_node);
      num_input_endpoints_list.push_back(1);
      input_buffer_management_schemes.push_back(buffer_management_scheme);
      input_buffer_size_list.push_back(enet_router_input_buffer_size);

      NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, core_interface);
      num_output_endpoints_list.push_back(1);
      downstream_buffer_management_schemes.push_back(BufferManagementScheme::INFINITE);
      downstream_buffer_size_list.push_back(-1);

      SInt32 cx, cy;
      computePosition(_core_id, cx, cy);

      // Add the adjoining emesh routers
      SInt32 dx[4] = {-1,1,0,0};
      SInt32 dy[4] = {0,0,-1,1};
      for (SInt32 i = 0; i < 4; i++)
      {
         core_id_t core_id = computeCoreID(cx+dx[i], cy+dy[i]);
         if (core_id != INVALID_CORE_ID)
         {
            Router::Id router_id(core_id, EMESH);

            NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, router_id);
            num_input_endpoints_list.push_back(1);
            input_buffer_management_schemes.push_back(buffer_management_scheme);
            input_buffer_size_list.push_back(enet_router_input_buffer_size);

            NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, router_id);
            num_output_endpoints_list.push_back(1);
            downstream_buffer_management_schemes.push_back(buffer_management_scheme);
            downstream_buffer_size_list.push_back(enet_router_input_buffer_size);
         }
      }

      // Add the hub, if it is an access point
      if (isAccessPoint(Router::Id(_core_id, node_type)))
      {
         Router::Id send_hub_router(computeCoreIDWithOpticalHub(_cluster_id), SEND_HUB);
        
         NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, send_hub_router); 
         num_output_endpoints_list.push_back(1);
         downstream_buffer_management_schemes.push_back(buffer_management_scheme);
         downstream_buffer_size_list.push_back(send_hub_router_input_buffer_size);
      }

      // Other parameters
      router_input_buffer_size = enet_router_input_buffer_size;
      router_data_pipeline_delay = enet_router_data_pipeline_delay;
      router_credit_pipeline_delay = enet_router_credit_pipeline_delay;
   }
   else if (node_type == SEND_HUB)
   {
      // Input channels from access point
      // Output channels to other hubs
      
      // Input Channels from access point
      vector<Router::Id> access_point_list = _cluster_info_list[_cluster_id]._access_point_list;
      vector<Router::Id>::iterator router_it = access_point_list.begin();
      for ( ; router_it != access_point_list.end(); router_it ++)
      {
         NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, *router_it);
         num_input_endpoints_list.push_back(1);
         input_buffer_management_schemes.push_back(buffer_management_scheme);
         input_buffer_size_list.push_back(send_hub_router_input_buffer_size);
      }
      
      // Output Channel to all receive hub
      // First, to receive hubs on other clusters
      vector<Router::Id> receive_hub_router_list;
      for (SInt32 i = 0; i < _num_clusters; i++)
      {
         Router::Id receive_hub_router(computeCoreIDWithOpticalHub(i), RECEIVE_HUB);
         receive_hub_router_list.push_back(receive_hub_router);
      }
      NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, receive_hub_router_list);
      num_output_endpoints_list.push_back(_num_clusters);
      downstream_buffer_management_schemes.push_back(buffer_management_scheme);
      downstream_buffer_size_list.push_back(receive_hub_router_input_buffer_size);
     
      // Other parameters 
      router_input_buffer_size = send_hub_router_input_buffer_size;
      if (_num_optical_access_points == 1)
      {
         router_data_pipeline_delay = 0;
         router_credit_pipeline_delay = 0;
      }
      else // (_num_optical_access_points > 1)
      {
         router_data_pipeline_delay = send_hub_router_data_pipeline_delay;
         router_credit_pipeline_delay = send_hub_router_credit_pipeline_delay;
      }
   }

   else if (node_type == RECEIVE_HUB)
   {
      // Output Channels to star_net router or cores (depending on receive_net type)
      // Inputs Channels from other hubs
      
      // Output channels to cores/star_net router
      if (_receive_net_type == STAR)
      {
         // _num_receive_nets_per_cluster routers, each with port configuration (1 x _cluster_size)
         for (SInt32 i = 0; i < _num_receive_nets_per_cluster; i++)
         {
            Router::Id star_net_router_id(computeCoreIDWithOpticalHub(_cluster_id), STAR_NET_ROUTER_BASE + i);
            NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, star_net_router_id);
            num_output_endpoints_list.push_back(1);
            downstream_buffer_management_schemes.push_back(buffer_management_scheme);
            downstream_buffer_size_list.push_back(star_net_router_input_buffer_size);
         }
      }

      else // (_receive_net_type == HTREE)
      {
         // Compute Core Interface List
         vector<Router::Id> core_interface_list;
         vector<core_id_t>::iterator core_it = core_id_list_in_cluster.begin();
         for ( ; core_it != core_id_list_in_cluster.end(); core_it ++)
         {
            Router::Id router_id(*core_it, CORE_INTERFACE);
            core_interface_list.push_back(router_id);
         }

         for (SInt32 i = 0; i < _num_receive_nets_per_cluster; i++)
         {
            NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, core_interface_list);
            num_output_endpoints_list.push_back(_cluster_size);
            downstream_buffer_management_schemes.push_back(BufferManagementScheme::INFINITE);
            downstream_buffer_size_list.push_back(-1);
         }
      }
      
      // Input channels from send hubs of all clusters
      for (SInt32 i = 0; i < _num_clusters; i++)
      {
         Router::Id send_hub_router(computeCoreIDWithOpticalHub(i), SEND_HUB);
         NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, send_hub_router);
         num_input_endpoints_list.push_back(1);
         input_buffer_management_schemes.push_back(buffer_management_scheme);
         input_buffer_size_list.push_back(receive_hub_router_input_buffer_size);
      }
      
      // Other parameters
      router_input_buffer_size = receive_hub_router_input_buffer_size;
      router_data_pipeline_delay = receive_hub_router_data_pipeline_delay;
      router_credit_pipeline_delay = receive_hub_router_credit_pipeline_delay;
   }

   else if ( (node_type >= STAR_NET_ROUTER_BASE) &&
             (node_type < (STAR_NET_ROUTER_BASE + _num_receive_nets_per_cluster)) )
   {
      LOG_ASSERT_ERROR(_receive_net_type == STAR, "Receive Net must be STAR");

      // Output Channels to cores
      // Compute Core Interface List
      vector<core_id_t>::iterator core_it = core_id_list_in_cluster.begin();
      for ( ; core_it != core_id_list_in_cluster.end(); core_it ++)
      {
         Router::Id output_router_id(*core_it, CORE_INTERFACE);
         NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, output_router_id);
         num_output_endpoints_list.push_back(1);
         downstream_buffer_management_schemes.push_back(BufferManagementScheme::INFINITE);
         downstream_buffer_size_list.push_back(-1);
      }

      // Input Channels
      // Only one from the RECEIVE_HUB
      Router::Id receive_hub_router(computeCoreIDWithOpticalHub(_cluster_id), RECEIVE_HUB);
      NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, receive_hub_router);
      num_input_endpoints_list.push_back(1);
      input_buffer_management_schemes.push_back(buffer_management_scheme);
      input_buffer_size_list.push_back(star_net_router_input_buffer_size);

      // Other Parameters
      router_input_buffer_size = star_net_router_input_buffer_size;
      router_data_pipeline_delay = star_net_router_data_pipeline_delay;
      router_credit_pipeline_delay = star_net_router_credit_pipeline_delay;
   }

   else
   {
      LOG_PRINT_ERROR("Unrecognized Router Type(%i)", node_type);
   }

   SInt32 num_input_channels = input_channel_to_router_id_list__mapping.size();
   SInt32 num_output_channels = output_channel_to_router_id_list__mapping.size();

   // Create Router Performance Model
   RouterPerformanceModel* router_performance_model =
         new RouterPerformanceModel(
             _flow_control_scheme,
             router_data_pipeline_delay,
             router_credit_pipeline_delay,
             num_input_channels, num_output_channels,
             num_input_endpoints_list, num_output_endpoints_list,
             input_buffer_management_schemes, downstream_buffer_management_schemes,
             input_buffer_size_list, downstream_buffer_size_list);

   // Create Router Power Model
   RouterPowerModel* router_power_model =
         RouterPowerModel::create(num_input_channels, num_output_channels,
                                  router_input_buffer_size, _flit_width);

   // Create Link Performance and Power Models
   vector<LinkPerformanceModel*> link_performance_model_list;
   vector<LinkPowerModel*> link_power_model_list;
   for (SInt32 channel_id = 0; channel_id < num_output_channels; channel_id ++)
   {
      LinkPerformanceModel* link_performance_model;
      LinkPowerModel* link_power_model;

      if (node_type == EMESH)
      {
         if ( (_num_optical_access_points == 1) && isAccessPoint(Router::Id(_core_id,node_type)) && (channel_id == (num_output_channels-1)) )
         {
            link_performance_model = NULL;
            link_power_model = NULL;
         }
         else
         {
            double link_length = _tile_width;
            link_performance_model = ElectricalLinkPerformanceModel::create(electrical_link_type,
                  _frequency, link_length, _flit_width, 1);
            assert(link_performance_model->getDelay() == 1);
            link_power_model = ElectricalLinkPowerModel::create(electrical_link_type,
                  _frequency, link_length, _flit_width, 1);
         }
      }

      else if (node_type == SEND_HUB)
      {
         double link_length = 10; // computeOpticalLinkLength();
         link_performance_model = new OpticalLinkPerformanceModel(_frequency,
               link_length, _flit_width, _num_clusters);
         LOG_ASSERT_ERROR(link_performance_model->getDelay() == 3,
                          "Expected Link Delay(3), Got(%llu)", link_performance_model->getDelay());
         link_power_model = new OpticalLinkPowerModel(_frequency,
               link_length, _flit_width, _num_clusters);
      }

      else if (node_type == RECEIVE_HUB)
      {
         if (_receive_net_type == HTREE) // HTREE Receive Net
         {
            double link_length = _tile_width * _cluster_size;
            link_performance_model = ElectricalLinkPerformanceModel::create(electrical_link_type,
                  _frequency, link_length, _flit_width, _cluster_size);
            assert(link_performance_model->getDelay() == 1);
            link_power_model = ElectricalLinkPowerModel::create(electrical_link_type,
                  _frequency, link_length, _flit_width, _cluster_size);
         }
         else // (_receive_net_type == STAR) - STAR Receive Net
         {
            link_performance_model = NULL;
            link_power_model = NULL;
         }
      }

      else if ( (node_type >= STAR_NET_ROUTER_BASE) &&
                (node_type < (STAR_NET_ROUTER_BASE + _num_receive_nets_per_cluster)) )
      {
         LOG_ASSERT_ERROR(_receive_net_type == STAR, "Only STAR Receive Net allowed");
         
         SInt32 distance = computeDistance(computeCoreIDWithOpticalHub(_cluster_id), core_id_list_in_cluster[channel_id]);
         double link_length = _tile_width * distance;
         if (link_length == 0.0)
            link_length = 0.1;      // Some small length
         link_performance_model = ElectricalLinkPerformanceModel::create(electrical_link_type,
               _frequency, link_length, _flit_width, 1);
         assert(link_performance_model->getDelay() == 1);
         link_power_model = ElectricalLinkPowerModel::create(electrical_link_type,
               _frequency, link_length, _flit_width, 1);
      }

      else
      {
         LOG_PRINT_ERROR("Unrecognized Router Type (%i)", node_type);
      }

      link_performance_model_list.push_back(link_performance_model);
      link_power_model_list.push_back(link_power_model);
   }
         
   return new NetworkNode(Router::Id(_core_id, node_type),
                          _flit_width,
                          router_performance_model,
                          router_power_model,
                          link_performance_model_list,
                          link_power_model_list,
                          input_channel_to_router_id_list__mapping,
                          output_channel_to_router_id_list__mapping,
                          _flow_control_packet_type);
}

void
FiniteBufferNetworkModelAtac::computeOutputEndpointList(HeadFlit* head_flit, NetworkNode* curr_network_node)
{
   LOG_PRINT("computeOutputEndpointList(%p, %p) start", head_flit, curr_network_node);
   Router::Id curr_router_id = curr_network_node->getRouterId();
   LOG_PRINT("Router Id (%i,%i)", curr_router_id._core_id, curr_router_id._index);
   
   assert(_core_id == curr_router_id._core_id);

   // Output Endpoint List
   vector<Channel::Endpoint> output_endpoint_vec;

   // First make the routing decision (ENet/ONet)
   // 1) ENet - go completely on ENet
   // 2) ONet - go over ENet, then ONet & finally ReceiveNet
   GlobalRoute global_route = computeGlobalRoute(head_flit->_sender, head_flit->_receiver);
   if (global_route == GLOBAL_ENET)
   {
      LOG_PRINT("Global Route: ENET");
      computeNextHopsOnENet(curr_network_node, head_flit->_sender, head_flit->_receiver,
            output_endpoint_vec);
   }
   else if (global_route == GLOBAL_ONET)
   {
      LOG_PRINT("Global Route: ONET");
      computeNextHopsOnONet(curr_network_node, head_flit->_sender, head_flit->_receiver,
            output_endpoint_vec);
   }

   head_flit->_output_endpoint_list = new vector<Channel::Endpoint>(output_endpoint_vec);
   
   LOG_PRINT("computeOutputEndpointList(%p, %p) end", head_flit, curr_network_node);
}

void
FiniteBufferNetworkModelAtac::computeNextHopsOnONet(NetworkNode* curr_network_node,
      core_id_t sender, core_id_t receiver,
      vector<Channel::Endpoint>& output_endpoint_vec)
{
   // What happens of receiver == NetPacket::BROADCAST
   // See if we are on the sender or receiver cluster
   Router::Id curr_router_id = curr_network_node->getRouterId();
   SInt32 curr_cluster_id = computeClusterID(curr_router_id._core_id);
   SInt32 sender_cluster_id = computeClusterID(sender);
   SInt32 node_type = curr_router_id._index;
 
   if (node_type == SEND_HUB)
   {
      if (receiver == NetPacket::BROADCAST)
      {
         // Channel - 0 is optical, Channel - 1 goes to the receive hub of same cluster
         // One channel -> 0, Broadcast -> Channel::Endpoint::ALL
         output_endpoint_vec.push_back(Channel::Endpoint(0, Channel::Endpoint::ALL));
      }
      else // (receiver != NetPacket::BROADCAST)
      {
         SInt32 receiver_cluster_id = computeClusterID(receiver);
         assert(receiver_cluster_id != sender_cluster_id);
         core_id_t core_id_with_hub = computeCoreIDWithOpticalHub(receiver_cluster_id);
         Router::Id receive_hub_router(core_id_with_hub, RECEIVE_HUB);

         LOG_PRINT("Receiver Router Id(%i,%i)", receive_hub_router._core_id, receive_hub_router._index);
         Channel::Endpoint output_endpoint =
               curr_network_node->getOutputEndpointFromRouterId(receive_hub_router);
         output_endpoint_vec.push_back(output_endpoint);
      }
   }

   else if ( (node_type == RECEIVE_HUB) ||
             ((node_type >= STAR_NET_ROUTER_BASE) && (node_type < (STAR_NET_ROUTER_BASE + _num_receive_nets_per_cluster))) )
   {
      if (_receive_net_type == HTREE)
         computeNextHopsOnHTree(curr_network_node, sender, receiver, output_endpoint_vec);
      else // (_receive_net_type == STAR)
         computeNextHopsOnStarNet(curr_network_node, sender, receiver, output_endpoint_vec);
   }

   else if (node_type == EMESH)
   {
      assert(curr_cluster_id == sender_cluster_id);
      // We are on the sending cluster
      // See if we are on the hub, access_point, or another core
      if (isAccessPoint(curr_router_id))
      {
         core_id_t core_id_with_hub = computeCoreIDWithOpticalHub(curr_cluster_id);
         Router::Id send_hub_router(core_id_with_hub, SEND_HUB);

         Channel::Endpoint output_endpoint =
               curr_network_node->getOutputEndpointFromRouterId(send_hub_router);
         output_endpoint_vec.push_back(output_endpoint);
      }
      else // (!isAccessPoint(curr_router_id))
      {
         Router::Id access_point = computeNearestAccessPoint(curr_router_id._core_id);
         // (sender, access_point /* receiver */, output_endpoint_vec)
         computeNextHopsOnENet(curr_network_node, sender, access_point._core_id, output_endpoint_vec);
      }
   }

   else
   {
      LOG_PRINT_ERROR("Unrecognized Node Type(%i)", node_type);
   }

 }

void
FiniteBufferNetworkModelAtac::computeNextHopsOnENet(NetworkNode* curr_network_node,
      core_id_t sender, core_id_t receiver,
      vector<Channel::Endpoint>& output_endpoint_vec)
{
   LOG_ASSERT_ERROR(receiver != NetPacket::BROADCAST, "ENet Routers not equipped for broadcast");
   Router::Id curr_router_id = curr_network_node->getRouterId();
   core_id_t curr_core_id = curr_router_id._core_id;
   
   SInt32 cx, cy;
   SInt32 dx, dy;
   computePosition(curr_core_id, cx, cy);
   computePosition(receiver, dx, dy);

   Router::Id next_dest;
   if (cx > dx)
      next_dest = Router::Id(computeCoreID(cx-1,cy), EMESH);
   else if (cx < dx)
      next_dest = Router::Id(computeCoreID(cx+1,cy), EMESH);
   else if (cy > dy)
      next_dest = Router::Id(computeCoreID(cx,cy-1), EMESH);
   else if (cy < dy)
      next_dest = Router::Id(computeCoreID(cx,cy+1), EMESH);
   else
      next_dest = Router::Id(_core_id, CORE_INTERFACE);

   // Convert "" router_id --> output_endpoint ""
   Channel::Endpoint output_endpoint = curr_network_node->getOutputEndpointFromRouterId(next_dest);
   output_endpoint_vec.push_back(output_endpoint);
}

void
FiniteBufferNetworkModelAtac::computeNextHopsOnHTree(NetworkNode* curr_network_node,
      core_id_t sender, core_id_t receiver,
      vector<Channel::Endpoint>& output_endpoint_vec)
{
   Router::Id curr_router_id = curr_network_node->getRouterId();
   SInt32 node_type = curr_router_id._index;
   LOG_ASSERT_ERROR(node_type == RECEIVE_HUB, "Is not RECEIVE_HUB");

   if (receiver == NetPacket::BROADCAST)
   {
      Channel::Endpoint output_endpoint(computeReceiveNetID(sender), Channel::Endpoint::ALL);
      output_endpoint_vec.push_back(output_endpoint);
   }
   else // (receiver != NetPacket::BROADCAST)
   {
      Router::Id core_interface(receiver, CORE_INTERFACE);
      Channel::Endpoint output_endpoint = curr_network_node->getOutputEndpointFromRouterId(core_interface);
      output_endpoint._channel_id = computeReceiveNetID(sender);
      output_endpoint_vec.push_back(output_endpoint);
   }
}

void
FiniteBufferNetworkModelAtac::computeNextHopsOnStarNet(NetworkNode* curr_network_node,
      core_id_t sender, core_id_t receiver,
      vector<Channel::Endpoint>& output_endpoint_vec)
{
   Router::Id curr_router_id = curr_network_node->getRouterId();
   SInt32 node_type = curr_router_id._index;
   
   if (node_type == RECEIVE_HUB)
   {
      // Both for unicasts and broadcasts
      Channel::Endpoint output_endpoint(computeReceiveNetID(sender), 0);
      output_endpoint_vec.push_back(output_endpoint);
   }

   else if ( (node_type >= STAR_NET_ROUTER_BASE) && (node_type < (STAR_NET_ROUTER_BASE + _num_receive_nets_per_cluster)) )
   {
      // STAR Receive Net router
      if (receiver == NetPacket::BROADCAST)
      {
         // All the output endpoints corresponding to this router
         for (SInt32 i = 0; i < _cluster_size; i++)
         {
            Channel::Endpoint output_endpoint(i, 0);
            output_endpoint_vec.push_back(output_endpoint);
         }
      }
      else // (receiver != NetPacket::BROADCAST)
      {
         // Just the output endpoint corresponding to the core to send to
         Router::Id core_interface(receiver, CORE_INTERFACE);
         Channel::Endpoint output_endpoint = curr_network_node->getOutputEndpointFromRouterId(core_interface);
         output_endpoint_vec.push_back(output_endpoint);
      }
   }
}

void
FiniteBufferNetworkModelAtac::initializeClusters()
{
   // Cluster -> Boundary and Access Point List
   _cluster_info_list.resize(_num_clusters);

   // Clusters
   // _numX_clusters, _numY_clusters, _cluster_width, _cluster_height
   if (isEven(floorLog2(_num_clusters)))
   {
      _numX_clusters = sqrt(_num_clusters);
      _numY_clusters = sqrt(_num_clusters);
   }
   else // (isOdd(floorLog2(_num_clusters)))
   {
      _numX_clusters = sqrt(_num_clusters/2);
      _numY_clusters = sqrt(_num_clusters*2);
   }
   _cluster_width = _enet_width / _numX_clusters;
   _cluster_height = _enet_height / _numY_clusters;

   // Initialize Cluster Boundaries
   for (SInt32 y = 0; y < _numY_clusters; y++)
   {
      for (SInt32 x = 0; x < _numX_clusters; x++)
      {
         SInt32 cluster_id = (y * _numX_clusters) + x;
         ClusterInfo::Boundary boundary(x * _cluster_width, (x+1) * _cluster_width,
                                        y * _cluster_height, (y+1) * _cluster_height);
         _cluster_info_list[cluster_id]._boundary = boundary;
      }
   }

   // Sub Clusters
   // _numX_sub_clusters, _numY_sub_clusters, _sub_cluster_width, _sub_cluster_height
   if (isEven(floorLog2(_num_sub_clusters)))
   {
      _numX_sub_clusters = sqrt(_num_sub_clusters);
      _numY_sub_clusters = sqrt(_num_sub_clusters);
   }
   else // (isOdd(floorLog2(_num_sub_clusters)))
   {
      _numX_sub_clusters = sqrt(_num_sub_clusters*2);
      _numY_sub_clusters = sqrt(_num_sub_clusters/2);
   }
   _sub_cluster_width = _cluster_width / _numX_sub_clusters;
   _sub_cluster_height = _cluster_height / _numY_sub_clusters;

   // Initialize Access Point List
   for (SInt32 i = 0; i < _num_clusters; i++)
   {
      initializeAccessPointList(i);
   }
}

void
FiniteBufferNetworkModelAtac::initializeAccessPointList(SInt32 cluster_id)
{
   ClusterInfo::Boundary& boundary = _cluster_info_list[cluster_id]._boundary;
   // Access Points
   // _access_point_list
   for (SInt32 y = 0; y < _numY_sub_clusters; y++)
   {
      for (SInt32 x = 0; x < _numX_sub_clusters; x++)
      {
         SInt32 access_point_x = boundary.minX + (x * _sub_cluster_width) + (_sub_cluster_width/2);
         SInt32 access_point_y = boundary.minY + (y * _sub_cluster_height) + (_sub_cluster_height/2);
         SInt32 access_point_id = access_point_y * _enet_width + access_point_x;
         _cluster_info_list[cluster_id]._access_point_list.push_back(Router::Id(access_point_id, EMESH));
      }
   }
}

core_id_t
FiniteBufferNetworkModelAtac::computeCoreID(SInt32 x, SInt32 y)
{
   if ((x < 0) || (y < 0) || (x >= _enet_width) || (y >= _enet_height))
      return INVALID_CORE_ID;
   else
      return (y * _enet_width + x);
}

void
FiniteBufferNetworkModelAtac::computePosition(core_id_t core_id, SInt32& x, SInt32& y)
{
   x = core_id % _enet_width;
   y = core_id / _enet_width;
}

SInt32
FiniteBufferNetworkModelAtac::computeDistance(core_id_t sender, core_id_t receiver)
{
   SInt32 sx, sy, dx, dy;
   computePosition(sender, sx, sy);
   computePosition(receiver, dx, dy);
   return (abs(sx-dx) + abs(sy-dy));
}

void
FiniteBufferNetworkModelAtac::computeCoreIDListInCluster(SInt32 cluster_id, vector<core_id_t>& core_id_list)
{
   ClusterInfo::Boundary& boundary = _cluster_info_list[cluster_id]._boundary;
   for (SInt32 y = 0; y < _cluster_height; y++)
   {
      for (SInt32 x = 0; x < _cluster_width; x++)
      {
         SInt32 pos_x = boundary.minX + x;
         SInt32 pos_y = boundary.minY + y;
         core_id_list.push_back(pos_y * _enet_width + pos_x);
      }
   }
}

SInt32
FiniteBufferNetworkModelAtac::computeClusterID(core_id_t core_id)
{
   SInt32 cx, cy;
   computePosition(core_id, cx, cy);

   SInt32 pos_x = cx / _cluster_width;
   SInt32 pos_y = cy / _cluster_height;
   return (pos_y * _numX_clusters) + pos_x;
}

SInt32
FiniteBufferNetworkModelAtac::computeSubClusterID(core_id_t core_id)
{
   SInt32 cx, cy;
   computePosition(core_id, cx, cy);

   SInt32 cluster_id = computeClusterID(core_id);
   // Get the cluster boundary
   ClusterInfo::Boundary& boundary = _cluster_info_list[cluster_id]._boundary;
   SInt32 pos_x = (cx - boundary.minX) / _sub_cluster_width;
   SInt32 pos_y = (cy - boundary.minY) / _sub_cluster_height;
   return (pos_y * _numX_sub_clusters) + pos_x;
}

Router::Id
FiniteBufferNetworkModelAtac::computeNearestAccessPoint(core_id_t core_id)
{
   SInt32 cluster_id = computeClusterID(core_id);
   SInt32 sub_cluster_id = computeSubClusterID(core_id);
   return _cluster_info_list[cluster_id]._access_point_list[sub_cluster_id];
}

bool
FiniteBufferNetworkModelAtac::isAccessPoint(Router::Id router_id)
{
   core_id_t core_id = router_id._core_id;
   return (router_id == computeNearestAccessPoint(core_id));
}

core_id_t
FiniteBufferNetworkModelAtac::computeCoreIDWithOpticalHub(SInt32 cluster_id)
{
   ClusterInfo::Boundary& boundary = _cluster_info_list[cluster_id]._boundary;
   SInt32 pos_x = boundary.minX + _cluster_width/2;
   SInt32 pos_y = boundary.minY + _cluster_height/2;
   return (pos_y * _enet_width + pos_x);
}

bool
FiniteBufferNetworkModelAtac::isHub(Router::Id router_id)
{
   return ((router_id._index == SEND_HUB) || (router_id._index == RECEIVE_HUB));
}

double
FiniteBufferNetworkModelAtac::computeOpticalLinkLength()
{
   // Note that number of clusters must be 'positive' and 'power of 2'
   // 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024

   if (_num_clusters == 2)
   {
      // Assume that optical link connects the mid-point of the clusters
      return (_cluster_height * _tile_width);
   }
   else if (_num_clusters == 4)
   {
      // Assume that optical link passes through mid-point of the clusters
      return (_cluster_width * _tile_width) * (_cluster_height * _tile_width);
   }
   else if (_num_clusters == 8)
   {
      return (_cluster_width * _tile_width) * (2 * _cluster_height * _tile_width);
   }
   else
   {
      // Assume that optical link passes through the edges of the clusters
      double length_rectangle = (_numX_clusters-2) * _cluster_width * _tile_width;
      double height_rectangle = (_cluster_height*2) * _tile_width;
      SInt32 num_rectangles = _numY_clusters/4;
      return (num_rectangles * 2 * (length_rectangle + height_rectangle));
   }
}

void
FiniteBufferNetworkModelAtac::outputSummary(ostream& out)
{
   FiniteBufferNetworkModel::outputSummary(out);
   outputEventCountSummary(out);
}

void
FiniteBufferNetworkModelAtac::outputEventCountSummary(ostream& out)
{
   // EMesh Routers
   // SendHub Routers
   // ReceiveHub Routers
   // if (StarNet):
   //    StarNet Routers
   // EMeshRouter-To-EMeshRouter Links
   // EMeshRouter-To-SendHub Links
   // SendHub-To-ReceiveHub Links
   // if (StarNet):
   //    ReceiveHub-To-StarNetRouter Links
   //    StarNetRouter-To-Core Links
   // elif (HTree):
   //    ReceiveHub-To-Cluster-HTree Links

   out << "  Event Counters:" << endl;
  
   // ENet Router 
   out << "   EMesh Router:" << endl;
   out << "    Input Buffer Writes: " << _network_node_map[EMESH]->getTotalInputBufferWrites() << endl;
   out << "    Input Buffer Reads: " << _network_node_map[EMESH]->getTotalInputBufferReads() << endl;
   out << "    Switch Allocator Requests: " << _network_node_map[EMESH]->getTotalSwitchAllocatorRequests() << endl;
   out << "    Crossbar Traversals: " << _network_node_map[EMESH]->getTotalCrossbarTraversals(1) << endl;
   
   if (_core_id == computeCoreIDWithOpticalHub(computeClusterID(_core_id)))
   {
      // Send Hub Router
      out << "   Send-Hub Router:" << endl;
      out << "    Input Buffer Writes: " << _network_node_map[SEND_HUB]->getTotalInputBufferWrites() << endl;
      out << "    Input Buffer Reads: " << _network_node_map[SEND_HUB]->getTotalInputBufferReads() << endl;
      out << "    Switch Allocator Requests: " << _network_node_map[SEND_HUB]->getTotalSwitchAllocatorRequests() << endl;
      out << "    Crossbar Traversals: " << _network_node_map[SEND_HUB]->getTotalCrossbarTraversals(1) << endl;

      // Receive Hub Router
      out << "   Receive-Hub Router:" << endl;
      out << "    Input Buffer Writes: " << _network_node_map[RECEIVE_HUB]->getTotalInputBufferWrites() << endl;
      out << "    Input Buffer Reads: " << _network_node_map[RECEIVE_HUB]->getTotalInputBufferReads() << endl;
      out << "    Switch Allocator Requests: " << _network_node_map[RECEIVE_HUB]->getTotalSwitchAllocatorRequests() << endl;
      out << "    Crossbar Traversals: " << _network_node_map[RECEIVE_HUB]->getTotalCrossbarTraversals(1) << endl;

      // Star Net Routers
      if (_receive_net_type == STAR)
      {
         for (SInt32 router_id = 0; router_id < _num_receive_nets_per_cluster; router_id ++)
         {
            NetworkNode* network_node = _network_node_map[STAR_NET_ROUTER_BASE + router_id];
            out << "   Star-Net Router (" << router_id << "):" << endl;
            out << "    Input Buffer Writes: " << network_node->getTotalInputBufferWrites() << endl;
            out << "    Input Buffer Reads: " << network_node->getTotalInputBufferReads() << endl;
            out << "    Switch Allocator Requests: " << network_node->getTotalSwitchAllocatorRequests() << endl;
            out << "    Crossbar Unicasts: " << network_node->getTotalCrossbarTraversals(1) << endl;
            out << "    Crossbar Broadcasts: " << network_node->getTotalCrossbarTraversals(_cluster_size) << endl;
         }
      }
   }
   else // Does not have optical hub
   {
      // Send Hub Router
      out << "   Send-Hub Router:" << endl;
      out << "    Input Buffer Writes:" << endl;
      out << "    Input Buffer Reads:" << endl;
      out << "    Switch Allocator Requests:" << endl;
      out << "    Crossbar Traversals:" << endl;

      // Receive Hub Router
      out << "   Receive-Hub Router:" << endl; 
      out << "    Input Buffer Writes:" << endl;
      out << "    Input Buffer Reads:" << endl;
      out << "    Switch Allocator Requests:" << endl;
      out << "    Crossbar Traversals:" << endl;

      // Star Net Routers
      if (_receive_net_type == STAR)
      {
         for (SInt32 router_id = 0; router_id < _num_receive_nets_per_cluster; router_id ++)
         {
            out << "   Star-Net Router (" << router_id << "):" << endl;
            out << "    Input Buffer Writes:" << endl;
            out << "    Input Buffer Reads:" << endl;
            out << "    Switch Allocator Requests:" << endl;
            out << "    Crossbar Unicasts:" << endl;
            out << "    Crossbar Broadcasts:" << endl;
         }
      }
   }

   out << "   Link Traversals:" << endl;
   // Link Traversals
   if (isAccessPoint(Router::Id(_core_id, EMESH)))
   {
      SInt32 num_output_channels = _network_node_map[EMESH]->getNumOutputChannels();
      UInt64 eMeshRouterToSendHubLinkTraversals = _network_node_map[EMESH]->getTotalOutputLinkUnicasts(num_output_channels-1);
      UInt64 eMeshRouterToEMeshRouterLinkTraversals = _network_node_map[EMESH]->getTotalOutputLinkUnicasts(Channel::ALL) - eMeshRouterToSendHubLinkTraversals;
   
      // EMeshRouter-To-EMeshRouter Link
      out << "    EMesh-Router To EMesh-Router Link Traversals: " << eMeshRouterToEMeshRouterLinkTraversals << endl;
   
      // EMeshRouter-To-SendHub Link
      out << "    EMesh-Router To Send-Hub Link Traversals: " << eMeshRouterToSendHubLinkTraversals << endl;
   }
   else // Not Access Point
   {
      UInt64 eMeshRouterToEMeshRouterLinkTraversals = _network_node_map[EMESH]->getTotalOutputLinkUnicasts(Channel::ALL);
   
      // EMeshRouter-To-EMeshRouter Link
      out << "    EMesh-Router To EMesh-Router Link Traversals: " << eMeshRouterToEMeshRouterLinkTraversals << endl;
   
      // EMeshRouter-To-SendHub Link
      out << "    EMesh-Router To Send-Hub Link Traversals:" << endl;
   }

   if (_core_id == computeCoreIDWithOpticalHub(computeClusterID(_core_id)))
   {
      // SendHub-To-ReceiveHub Link
      out << "    Send-Hub To Receive-Hub Link Unicasts: " << _network_node_map[SEND_HUB]->getTotalOutputLinkUnicasts(0) << endl;
      out << "    Send-Hub To Receive-Hub Link Broadcasts: " << _network_node_map[SEND_HUB]->getTotalOutputLinkBroadcasts(0) << endl;

      for (SInt32 receive_net = 0; receive_net < _num_receive_nets_per_cluster; receive_net ++)
      {
         if (_receive_net_type == STAR)
         {
            // ReceiveHub-To-StarNetRouter Link
            out << "    Receive-Hub To Star-Net Router (" << receive_net << ") Link Traversals: " << _network_node_map[RECEIVE_HUB]->getTotalOutputLinkUnicasts(receive_net) << endl;
            
            // StarNetRouter-To-Core Link
            out << "    Star-Net Router (" << receive_net << ") To Core Link Traversals: " << _network_node_map[STAR_NET_ROUTER_BASE + receive_net]->getTotalOutputLinkUnicasts(Channel::ALL) << endl;
         }

         else // (_receive_net_type == HTREE) - HTREE Receive Net
         {
            // ReceiveHub-To-Cluster-HTree Links
            UInt64 htree_traversals = _network_node_map[RECEIVE_HUB]->getTotalOutputLinkUnicasts(receive_net) +
                                      _network_node_map[RECEIVE_HUB]->getTotalOutputLinkBroadcasts(receive_net);
            out << "    Receive-Hub To Cluster HTree (" << receive_net << ") Traversals: " << htree_traversals << endl;
         }
      }
   }

   else // No Optical Hub
   {
      // SendHub-To-ReceiveHub Link
      out << "    Send-Hub To Receive-Hub Link Unicasts:" << endl;
      out << "    Send-Hub To Receive-Hub Link Broadcasts:" << endl;

      for (SInt32 receive_net = 0; receive_net < _num_receive_nets_per_cluster; receive_net ++)
      {
         if (_receive_net_type == STAR)
         {
            // ReceiveHub-To-StarNetRouter Link
            out << "    Receive-Hub To Star-Net Router (" << receive_net << ") Link Traversals:" << endl;

            // StarNetRouter-To-Core Link
            out << "    Star-Net Router (" << receive_net << ") To Core Link Traversals:" << endl;
         }

         else // (_receive_net_type == HTREE) - HTREE Receive Net
         {
            // ReceiveHub-To-Cluster-HTree Links
            out << "    Receive-Hub To Cluster HTree (" << receive_net << ") Traversals:" << endl;
         }
      }
   }
}

pair<bool,SInt32>
FiniteBufferNetworkModelAtac::computeCoreCountConstraints(SInt32 core_count)
{
   return make_pair(false, core_count);
}

pair<bool,vector<core_id_t> >
FiniteBufferNetworkModelAtac::computeMemoryControllerPositions(SInt32 num_memory_controllers)
{
   // Initialize the topology parameters in case called by an external model
   initializeANetTopologyParameters();

   LOG_ASSERT_ERROR(num_memory_controllers <= _num_clusters,
         "num_memory_controllers(%i), num_clusters(%i)", num_memory_controllers, _num_clusters);

   vector<core_id_t> core_id_list_with_memory_controllers;
   for (SInt32 i = 0; i < num_memory_controllers; i++)
   {
      core_id_list_with_memory_controllers.push_back(computeCoreIDWithOpticalHub(i));
   }

   return (make_pair(true, core_id_list_with_memory_controllers));
}
