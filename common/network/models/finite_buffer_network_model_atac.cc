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
#include "log.h"

// Static Variables
SInt32 FiniteBufferNetworkModelAtac::_enet_width;
SInt32 FiniteBufferNetworkModelAtac::_enet_height;
SInt32 FiniteBufferNetworkModelAtac::_num_clusters;
SInt32 FiniteBufferNetworkModelAtac::_cluster_size;
SInt32 FiniteBufferNetworkModelAtac::_sqrt_cluster_size;

FiniteBufferNetworkModelAtac::FiniteBufferNetworkModelAtac(Network* network, SInt32 network_id):
   FiniteBufferNetworkModel(network, network_id)
{
   // Initialize ANet topology related parameters
   initializeANetTopologyParameters();

   // Get the configuration parameters
   try
   {
      _frequency = Sim()->getCfg()->getFloat("network/atac/frequency");
      _flit_width = Sim()->getCfg()->getInt("network/atac/flit_width");
      _flow_control_scheme = FlowControlScheme::parse(
            Sim()->getCfg()->getString("network/atac/flow_control_scheme"));

      _num_bnets_in_cluster = Sim()->getCfg()->getInt("network/atac/num_bnets_in_cluster");
      
      // Global Routing Strategy
      // Local Broadcast Route
      // Local Unicast Route
      _global_routing_strategy = parseGlobalRoutingStrategy(
            Sim()->getCfg()->getString("network/atac/global_routing_strategy"));
      _unicast_distance_threshold = Sim()->getCfg()->getInt(
            "network/atac/unicast_distance_threshold");
      _local_unicast_route = parseLocalRoute(
            Sim()->getCfg()->getString("network/atac/local_unicast_route"));
      _local_broadcast_route = parseLocalRoute(
            Sim()->getCfg()->getString("network/atac/local_broadcast_route"));
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read Atac model parameters from the config file");
   }

   // Cluster Id
   _cluster_id = computeClusterId(_core_id);
   
   // Create a list of access points
   createAccessPointList();

   // Create the routers
   _network_node_list.push_back(createNetworkNode(EMESH));
   if (_core_id == getCoreIdWithOpticalHub(_cluster_id))
   {
      _network_node_list.push_back(createNetworkNode(SENDING_HUB));
      _network_node_list.push_back(createNetworkNode(RECEIVING_HUB));
   }
}

FiniteBufferNetworkModelAtac::~FiniteBufferNetworkModelAtac()
{
   // Delete the router objects
   vector<NetworkNode*>::iterator it = _network_node_list.begin();
   for ( ; it != _network_node_list.end(); it ++)
      delete (*it);
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
      return NUM_GLOBAL_ROUTING_STRATEGIES;
   }
}

FiniteBufferNetworkModelAtac::LocalRoute
FiniteBufferNetworkModelAtac::parseLocalRoute(string str)
{
   if (str == "enet")
      return LOCAL_ENET;
   else if (str == "bnet")
      return LOCAL_BNET;
   else
   {
      LOG_PRINT_ERROR("Unrecongized local route(%s)", str.c_str());
      return NUM_LOCAL_ROUTES;
   }
}

void
FiniteBufferNetworkModelAtac::initializeANetTopologyParameters()
{
   SInt32 total_cores = Config::getSingleton()->getTotalCores();

   try
   {
      _cluster_size = Sim()->getCfg()->getInt("network/atac/cluster_size");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Error reading atac cluster size");
   }

   // Cluster Size
   _sqrt_cluster_size = (SInt32) floor(sqrt(_cluster_size));
   LOG_ASSERT_ERROR(_cluster_size == (_sqrt_cluster_size * _sqrt_cluster_size),
         "Cluster Size(%i) must be a perfect square", _cluster_size);

   // Calculations with an electrical mesh
   _enet_width = (SInt32) floor(sqrt(total_cores));
   _enet_height = (SInt32) ceil(1.0 * total_cores / _enet_width);
   LOG_ASSERT_ERROR(_enet_width % _sqrt_cluster_size == 0, \
         "Mesh Width(%i) must be a multiple of sqrt_cluster_size(%i)", \
         _enet_width, _sqrt_cluster_size);
   LOG_ASSERT_ERROR(_enet_height == _enet_width,
         "Mesh Width(%i), Mesh Height(%i)", _enet_width, _enet_height);
   LOG_ASSERT_ERROR((_enet_width * _enet_height) == (SInt32) total_cores,
         "Mesh Width(%i), Mesh Height(%i), Core Count(%i)", \
         _enet_width, _enet_height, total_cores);

   // Number of Clusters
   _num_clusters = (_enet_width / _sqrt_cluster_size) * (_enet_height / _sqrt_cluster_size);
}

void
FiniteBufferNetworkModelAtac::createAccessPointList()
{
   SInt32 cluster_width = computeClusterWidth();
   SInt32 cluster_height = computeClusterHeight();
   // Each core will have information about its cluster
   core_id_t core_id_with_optical_hub = getCoreIdWithOpticalHub(_cluster_id);
   SInt32 hx, hy;
   computeENetPosition(core_id_with_optical_hub, hx, hy);
   
   core_id_t lb_access_point = computeCoreId( \
         hx + (cluster_width-1)/2, hy + (cluster_height-1)/2);
   _access_point_list.push_back(Router::Id(lb_access_point, EMESH));

   if (cluster_width > 1)
   {
      core_id_t rb_access_point = computeCoreId( \
            hx + (cluster_width+1)/2, hy + (cluster_height-1)/2);
      _access_point_list.push_back(Router::Id(rb_access_point, EMESH));
   }

   if (cluster_height > 1)
   {
      core_id_t lt_access_point = computeCoreId( \
            hx + (cluster_width-1)/2, hy + (cluster_height+1)/2);
      _access_point_list.push_back(Router::Id(lt_access_point, EMESH));
   }

   if ((cluster_width > 1) && (cluster_height > 1))
   {
      core_id_t rt_access_point = computeCoreId( \
            hx + (cluster_width+1)/2, hy + (cluster_height+1)/2);
      _access_point_list.push_back(Router::Id(rt_access_point, EMESH));
   }
}

SInt32
FiniteBufferNetworkModelAtac::computeClusterWidth()
{
   return (_sqrt_cluster_size);
}

SInt32
FiniteBufferNetworkModelAtac::computeClusterHeight()
{
   core_id_t core_id_with_optical_hub = getCoreIdWithOpticalHub(_cluster_id);
   SInt32 core_x, core_y;
   computeENetPosition(core_id_with_optical_hub, core_x, core_y);
   
   if (core_y >= _enet_width)
      return (_enet_height - _enet_width);
   else
      return (_sqrt_cluster_size);
}

NetworkNode*
FiniteBufferNetworkModelAtac::createNetworkNode(NodeType node_type)
{
   // Read Network Parameters
   BufferManagementScheme::Type buffer_management_scheme;
   
   // Router Parameters
   SInt32 enet_router_input_buffer_size = 0;
   SInt32 enet_router_data_pipeline_delay = 0;
   SInt32 enet_router_credit_pipeline_delay = 0;

   SInt32 receiving_hub_router_input_buffer_size = 0;
   SInt32 receiving_hub_router_data_pipeline_delay = 0;
   SInt32 receiving_hub_router_credit_pipeline_delay = 0;
  
   // Link Parameters 
   string enet_link_type;
   volatile double enet_link_length = 0.0;
   
   volatile double onet_link_length = 0.0;

   string bnet_link_type;
   volatile double bnet_link_length = 0.0;

   try
   {
      buffer_management_scheme = BufferManagementScheme::parse(
            Sim()->getCfg()->getString("network/atac/buffer_management_scheme"));
      
      enet_router_input_buffer_size = Sim()->getCfg()->getInt(
            "network/atac/enet/router/input_buffer_size");
      enet_router_data_pipeline_delay = Sim()->getCfg()->getInt(
            "network/atac/enet/router/data_pipeline_delay");
      enet_router_credit_pipeline_delay = Sim()->getCfg()->getInt(
            "network/atac/enet/router/credit_pipeline_delay");
      
      enet_link_type = Sim()->getCfg()->getString("network/atac/enet/link/type");
      enet_link_length = Sim()->getCfg()->getFloat("network/atac/enet/link/length");

      receiving_hub_router_input_buffer_size = Sim()->getCfg()->getInt(
            "network/atac/bnet/router/input_buffer_size");
      receiving_hub_router_data_pipeline_delay = Sim()->getCfg()->getInt(
            "network/atac/bnet/router/data_pipeline_delay");
      receiving_hub_router_credit_pipeline_delay = Sim()->getCfg()->getInt(
            "network/atac/bnet/router/credit_pipeline_delay");

      onet_link_length = Sim()->getCfg()->getFloat("network/atac/onet/link/length");

      bnet_link_type = Sim()->getCfg()->getString("network/atac/bnet/link/type");
      bnet_link_length = Sim()->getCfg()->getFloat("network/atac/bnet/link/length");
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

   // Other parameters 
   SInt32 router_input_buffer_size = 0;
   SInt32 router_data_pipeline_delay = 0;
   SInt32 router_credit_pipeline_delay = 0;

   // Core Id list in the current cluster
   vector<core_id_t> core_id_list_in_cluster;
   computeCoreIdListInCluster(_cluster_id, core_id_list_in_cluster);
   
   if (node_type == EMESH)
   {
      // Add the core interface
      Router::Id core_interface(_core_id, CORE_INTERFACE);

      NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, core_interface);
      num_input_endpoints_list.push_back(1);
      input_buffer_management_schemes.push_back(BufferManagementScheme::INFINITE);
      input_buffer_size_list.push_back(-1);

      NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, core_interface);
      num_output_endpoints_list.push_back(1);
      downstream_buffer_management_schemes.push_back(BufferManagementScheme::INFINITE);
      downstream_buffer_size_list.push_back(-1);

      SInt32 cx, cy;
      computeENetPosition(_core_id, cx, cy);

      // Add the adjoining emesh routers
      SInt32 dx[4] = {-1,1,0,0};
      SInt32 dy[4] = {0,0,-1,1};
      for (SInt32 i = 0; i < 4; i++)
      {
         core_id_t core_id = computeCoreId(cx+dx[i], cy+dy[i]);
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
         Router::Id sending_router_at_hub(getCoreIdWithOpticalHub(_cluster_id), SENDING_HUB);
         Router::Id receiving_router_at_hub(getCoreIdWithOpticalHub(_cluster_id), RECEIVING_HUB);
        
         NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, receiving_router_at_hub);
         num_input_endpoints_list.push_back(1);
         input_buffer_management_schemes.push_back(buffer_management_scheme);
         input_buffer_size_list.push_back(enet_router_input_buffer_size);

         NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, sending_router_at_hub); 
         num_output_endpoints_list.push_back(1);
         downstream_buffer_management_schemes.push_back(buffer_management_scheme);
         downstream_buffer_size_list.push_back(enet_router_input_buffer_size);
      }

      // Other parameters
      router_input_buffer_size = enet_router_input_buffer_size;
      router_data_pipeline_delay = enet_router_data_pipeline_delay;
      router_credit_pipeline_delay = enet_router_credit_pipeline_delay;
   }
   else if (node_type == SENDING_HUB)
   {
      // Input channels from access point
      // Output channels to other hubs
      
      // Input Channels from access point
      vector<Router::Id>::iterator router_it = _access_point_list.begin();
      for ( ; router_it != _access_point_list.end(); router_it ++)
      {
         NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, *router_it);
         num_input_endpoints_list.push_back(1);
         input_buffer_management_schemes.push_back(buffer_management_scheme);
         input_buffer_size_list.push_back(enet_router_input_buffer_size);
      }
      
      // Output Channel to other hubs
      vector<Router::Id> router_at_hub_list;
      for (SInt32 i = 0; i < _num_clusters; i++)
      {
         if (i != _cluster_id)
         {
            Router::Id router_at_hub(getCoreIdWithOpticalHub(i), RECEIVING_HUB);
            router_at_hub_list.push_back(router_at_hub);
         }
      }
      NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, router_at_hub_list);
      num_output_endpoints_list.push_back(_num_clusters-1);
      downstream_buffer_management_schemes.push_back(buffer_management_scheme);
      downstream_buffer_size_list.push_back(receiving_hub_router_input_buffer_size);
     
      // Other parameters 
      router_input_buffer_size = enet_router_input_buffer_size;
      router_data_pipeline_delay = enet_router_data_pipeline_delay;
      router_credit_pipeline_delay = enet_router_credit_pipeline_delay;
   }
   else if (node_type == RECEIVING_HUB)
   {
      // Output Channels to access points and cores
      // Inputs Channels from other hubs
      
      // Output channels to cores
      vector<Router::Id> core_interface_list;
      vector<core_id_t>::iterator core_it = core_id_list_in_cluster.begin();
      for ( ; core_it != core_id_list_in_cluster.end(); core_it ++)
      {
         Router::Id router_id(*core_it, CORE_INTERFACE);
         core_interface_list.push_back(router_id);
      }
      for (SInt32 i = 0; i < _num_bnets_in_cluster; i++)
      {
         NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, core_interface_list);
         num_output_endpoints_list.push_back(core_id_list_in_cluster.size());
         downstream_buffer_management_schemes.push_back(BufferManagementScheme::INFINITE);
         downstream_buffer_size_list.push_back(-1);
      }
      
      // Output channels to access point
      vector<Router::Id>::iterator router_it = _access_point_list.begin();
      for ( ; router_it != _access_point_list.end(); router_it ++)
      {
         NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, *router_it);
         num_output_endpoints_list.push_back(1);
         downstream_buffer_management_schemes.push_back(buffer_management_scheme);
         downstream_buffer_size_list.push_back(enet_router_input_buffer_size);
      }

      // Input channels from other hubs
      for (SInt32 i = 0; i < _num_clusters; i++)
      {
         if (i != _cluster_id)
         {
            Router::Id router_at_hub(getCoreIdWithOpticalHub(i), SENDING_HUB);
            NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, router_at_hub);
            num_input_endpoints_list.push_back(1);
            input_buffer_management_schemes.push_back(buffer_management_scheme);
            input_buffer_size_list.push_back(receiving_hub_router_input_buffer_size);
         }
      }
      
      // Other parameters
      router_input_buffer_size = receiving_hub_router_input_buffer_size;
      router_data_pipeline_delay = receiving_hub_router_data_pipeline_delay;
      router_credit_pipeline_delay = receiving_hub_router_credit_pipeline_delay;
   }

   SInt32 num_input_channels = input_channel_to_router_id_list__mapping.size();
   SInt32 num_output_channels = output_channel_to_router_id_list__mapping.size();

   // Create Router Performance Model
   RouterPerformanceModel* router_performance_model = \
         new RouterPerformanceModel( \
               _flow_control_scheme, \
               router_data_pipeline_delay, \
               router_credit_pipeline_delay, \
               num_input_channels, num_output_channels, \
               num_input_endpoints_list, num_output_endpoints_list, \
               input_buffer_management_schemes, downstream_buffer_management_schemes, \
               input_buffer_size_list, downstream_buffer_size_list);

   // Create Router Power Model
   RouterPowerModel* router_power_model = \
         RouterPowerModel::create(num_input_channels, num_output_channels, \
               router_input_buffer_size, _flit_width);

   // Create Link Performance and Power Models
   vector<LinkPerformanceModel*> link_performance_model_list;
   vector<LinkPowerModel*> link_power_model_list;
   for (SInt32 i = 0; i < num_output_channels; i++)
   {
      LinkPerformanceModel* link_performance_model;
      LinkPowerModel* link_power_model;

      if (node_type == EMESH)
      {
         link_performance_model = ElectricalLinkPerformanceModel::create(enet_link_type, \
               _frequency, enet_link_length, _flit_width, 1);
         link_power_model = ElectricalLinkPowerModel::create(enet_link_type, \
               _frequency, enet_link_length, _flit_width, 1);
      }
      else if (node_type == SENDING_HUB)
      {
         link_performance_model = new OpticalLinkPerformanceModel(_frequency, \
               onet_link_length, _flit_width, _num_clusters - 1);
         link_power_model = new OpticalLinkPowerModel(_frequency, \
               onet_link_length, _flit_width, _num_clusters - 1);
      }
      else if (node_type == RECEIVING_HUB)
      {
         if (i < _num_bnets_in_cluster)
         {
            SInt32 num_cores_in_cluster = core_id_list_in_cluster.size();
            link_performance_model = ElectricalLinkPerformanceModel::create(bnet_link_type, \
                  _frequency, bnet_link_length, _flit_width, num_cores_in_cluster);
            link_power_model = ElectricalLinkPowerModel::create(bnet_link_type, \
                  _frequency, bnet_link_length, _flit_width, num_cores_in_cluster);
         }
         else
         {
            link_performance_model = ElectricalLinkPerformanceModel::create(enet_link_type, \
                  _frequency, enet_link_length, _flit_width, 1);
            link_power_model = ElectricalLinkPowerModel::create(enet_link_type, \
                  _frequency, enet_link_length, _flit_width, 1);
         }
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
FiniteBufferNetworkModelAtac::computeOutputEndpointList(Flit* head_flit, NetworkNode* curr_network_node)
{
   LOG_PRINT("computeOutputEndpointList(%p, %p) start", head_flit, curr_network_node);
   Router::Id curr_router_id = curr_network_node->getRouterId();
   LOG_PRINT("Router Id [%i,%i])", curr_router_id._core_id, curr_router_id._index);
   
   assert(_core_id == curr_router_id._core_id);

   // Output Endpoint List
   vector<Channel::Endpoint> output_endpoint_vec;

   // First make the routing decision (ENet/ONet)
   // 1) ENet - go completely on ENet
   // 2) ONet - go over a mix of ENet, ONet & BNet
   GlobalRoute global_route = computeGlobalRoute(head_flit->_sender, head_flit->_receiver);
   if (global_route == GLOBAL_ENET)
   {
      LOG_PRINT("Global Route: ENET");
      computeNextHopsOnENet(curr_network_node, head_flit->_sender, head_flit->_receiver, \
            output_endpoint_vec);
   }
   else if (global_route == GLOBAL_ONET)
   {
      LOG_PRINT("Global Route: ONET");
      computeNextHopsOnONet(curr_network_node, head_flit->_sender, head_flit->_receiver, \
            output_endpoint_vec);
   }

   head_flit->_output_endpoint_list = new ChannelEndpointList(output_endpoint_vec);
   
   LOG_PRINT("computeOutputEndpointList(%p, %p) end", head_flit, curr_network_node);
}

FiniteBufferNetworkModelAtac::GlobalRoute
FiniteBufferNetworkModelAtac::computeGlobalRoute(core_id_t sender, core_id_t receiver)
{
   if (receiver == NetPacket::BROADCAST)
      return GLOBAL_ONET;

   // Unicast packet
   if (_global_routing_strategy == DISTANCE_BASED)
   {
      SInt32 distance = computeDistanceOnENet(sender, receiver);
      if (distance <= _unicast_distance_threshold)
         return GLOBAL_ENET;
      else
         return GLOBAL_ONET;
   }
   else // cluster-based global routing strategy
   {
      if (computeClusterId(sender) == computeClusterId(receiver))
         return GLOBAL_ENET;
      else
         return GLOBAL_ONET;
   }
}

FiniteBufferNetworkModelAtac::LocalRoute
FiniteBufferNetworkModelAtac::computeLocalRoute(core_id_t receiver)
{
   if (receiver == NetPacket::BROADCAST)
      return _local_broadcast_route;
   else // (receiver != NetPacket::BROADCAST)
      return _local_unicast_route;
}

SInt32
FiniteBufferNetworkModelAtac::getBNetChannelId(core_id_t sender)
{
   SInt32 sending_cluster_id = computeClusterId(sender);
   return (sending_cluster_id % _num_bnets_in_cluster);
}

void
FiniteBufferNetworkModelAtac::computeNextHopsOnONet(NetworkNode* curr_network_node,
      core_id_t sender, core_id_t receiver,
      vector<Channel::Endpoint>& output_endpoint_vec)
{
   // See if we are on the sender or receiver cluster
   Router::Id curr_router_id = curr_network_node->getRouterId();
   SInt32 curr_cluster_id = computeClusterId(curr_router_id._core_id);
   SInt32 receiver_cluster_id = computeClusterId(receiver);

   if (curr_cluster_id == receiver_cluster_id)
   {
      if (isHub(curr_router_id))
      {
         LocalRoute local_route = computeLocalRoute(receiver);
         if (local_route == LOCAL_BNET)
         {
            computeNextHopsOnBNet(curr_network_node, sender, receiver, output_endpoint_vec);
         }
         else // (local_route == LOCAL_ENET)
         {
            if (receiver == NetPacket::BROADCAST)
            {
               vector<Router::Id>::iterator it = _access_point_list.begin();
               for ( ; it != _access_point_list.end(); it ++)
               {
                  Channel::Endpoint& output_endpoint = \
                        curr_network_node->getOutputEndpointFromRouterId(*it);
                  output_endpoint_vec.push_back(output_endpoint);
               }
            }
            else // (receiver != NetPacket::BROADCAST)
            {
               Router::Id& access_point = getNearestAccessPoint(receiver);
               Channel::Endpoint& output_endpoint = \
                     curr_network_node->getOutputEndpointFromRouterId(access_point);
               output_endpoint_vec.push_back(output_endpoint);
            }
         }
      }
      else // (!isHub(curr_router_id))
      {
         // Route is ENet by default since BNet takes the flit directly to the core
         // (curr_network_node, sender, receiver, next_hop_list)
         Router::Id& access_point = getNearestAccessPoint(curr_router_id._core_id);
         computeNextHopsOnENet(curr_network_node, access_point._core_id, receiver, output_endpoint_vec);
      }
   }

   else // (curr_cluster_id != receiver_cluster_id) -> Sender Cluster
   {
      // We are on the sending cluster
      // See if we are on the hub, access_point, or another core
      if (isHub(curr_router_id))
      {
         if (receiver == NetPacket::BROADCAST)
         {
            // One channel -> 0, Broadcast -> Channel::Endpoint::ALL
            Channel::Endpoint output_endpoint(0, Channel::Endpoint::ALL);
            output_endpoint_vec.push_back(output_endpoint);
         }
         else // (receiver != NetPacket::BROADCAST)
         {
            UInt32 receiver_cluster_id = computeClusterId(receiver);
            core_id_t core_id_with_hub = getCoreIdWithOpticalHub(receiver_cluster_id);
            Router::Id receiver_router_id(core_id_with_hub, RECEIVING_HUB);

            LOG_PRINT("Receiver Router Id(%i,%i)", receiver_router_id._core_id, receiver_router_id._index);
            Channel::Endpoint& output_endpoint = \
                  curr_network_node->getOutputEndpointFromRouterId(receiver_router_id);
            output_endpoint_vec.push_back(output_endpoint);
         }
      }
      else if (isAccessPoint(curr_router_id))
      {
         core_id_t core_id_with_hub = getCoreIdWithOpticalHub(curr_cluster_id);
         Router::Id receiver_router_id(core_id_with_hub, SENDING_HUB);

         Channel::Endpoint& output_endpoint = \
               curr_network_node->getOutputEndpointFromRouterId(receiver_router_id);
         output_endpoint_vec.push_back(output_endpoint);
      }
      else // (!isHub(curr_router_id) && !isAccessPoint(curr_router_id))
      {
         Router::Id access_point = getNearestAccessPoint(curr_router_id._core_id);
         // (sender, access_point /* receiver */, output_endpoint_vec)
         computeNextHopsOnENet(curr_network_node, sender, access_point._core_id, output_endpoint_vec);
      }
   }
}

void
FiniteBufferNetworkModelAtac::computeNextHopsOnENet(NetworkNode* curr_network_node,
      core_id_t sender, core_id_t receiver,
      vector<Channel::Endpoint>& output_endpoint_vec)
{
   Router::Id curr_router_id = curr_network_node->getRouterId();
   core_id_t curr_core_id = curr_router_id._core_id;
   SInt32 cx, cy;
   computeENetPosition(curr_core_id, cx, cy);

   list<Router::Id> next_dest_list;

   if (receiver == NetPacket::BROADCAST)
   {
      SInt32 sx, sy;
      computeENetPosition(sender, sx, sy);

      if (cy >= sy)
         next_dest_list.push_back(Router::Id(computeCoreId(cx,cy+1), EMESH));
      if (cy <= sy)
         next_dest_list.push_back(Router::Id(computeCoreId(cx,cy-1), EMESH));
      if (cy == sy)
      {
         if (cx >= sx)
            next_dest_list.push_back(Router::Id(computeCoreId(cx+1,cy), EMESH));
         if (cx <= sx)
            next_dest_list.push_back(Router::Id(computeCoreId(cx-1,cy), EMESH));
         if (cx == sx)
            next_dest_list.push_back(Router::Id(_core_id, CORE_INTERFACE));
      }

      // Eliminate cores that are not present in the same cluster and sub-cluster
      list<Router::Id>::iterator it = next_dest_list.begin();
      while (it != next_dest_list.end())
      {
         core_id_t next_core_id = (*it)._core_id;
         if ( (next_core_id == INVALID_CORE_ID) ||
              (computeClusterId(next_core_id) != computeClusterId(curr_core_id)) ||
              (computeSubClusterId(next_core_id) != computeSubClusterId(curr_core_id)) )
         {
            it = next_dest_list.erase(it);
         }
         else
         {
            it ++;
         }
      }
   }
   else
   {
      SInt32 dx, dy;
      computeENetPosition(receiver, dx, dy);

      if (cx > dx)
         next_dest_list.push_back(Router::Id(computeCoreId(cx-1,cy), EMESH));
      else if (cx < dx)
         next_dest_list.push_back(Router::Id(computeCoreId(cx+1,cy), EMESH));
      else if (cy > dy)
         next_dest_list.push_back(Router::Id(computeCoreId(cx,cy-1), EMESH));
      else if (cy < dy)
         next_dest_list.push_back(Router::Id(computeCoreId(cx,cy+1), EMESH));
      else
         next_dest_list.push_back(Router::Id(_core_id, CORE_INTERFACE));
   }

   // Convert "" core_id --> router_id --> output_endpoint ""
   list<Router::Id>::iterator it = next_dest_list.begin();
   for ( ; it != next_dest_list.end(); it ++)
   {
      Channel::Endpoint& output_endpoint = curr_network_node->getOutputEndpointFromRouterId(*it);
      output_endpoint_vec.push_back(output_endpoint);
   }
}

void
FiniteBufferNetworkModelAtac::computeNextHopsOnBNet(NetworkNode* curr_network_node,
      core_id_t sender, core_id_t receiver,
      vector<Channel::Endpoint>& output_endpoint_vec)
{
   if (receiver == NetPacket::BROADCAST)
   {
      Channel::Endpoint output_endpoint(getBNetChannelId(sender), Channel::Endpoint::ALL);
      output_endpoint_vec.push_back(output_endpoint);
   }
   else // (receiver != NetPacket::BROADCAST)
   {
      Router::Id receiver_router_id(receiver, CORE_INTERFACE);
      Channel::Endpoint& output_endpoint = \
            curr_network_node->getOutputEndpointFromRouterId(receiver_router_id);
      output_endpoint_vec.push_back(output_endpoint);
   }
}

core_id_t
FiniteBufferNetworkModelAtac::computeCoreId(SInt32 x, SInt32 y)
{
   if ((x >= _enet_width) || (y >= _enet_height))
      return INVALID_CORE_ID;
   else
      return (y * _enet_width + x);
}

void
FiniteBufferNetworkModelAtac::computeENetPosition(core_id_t core_id, SInt32& x, SInt32& y)
{
   x = core_id % _enet_width;
   y = core_id / _enet_width;
}

SInt32
FiniteBufferNetworkModelAtac::computeDistanceOnENet(core_id_t sender, core_id_t receiver)
{
   SInt32 sx, sy, dx, dy;
   computeENetPosition(sender, sx, sy);
   computeENetPosition(receiver, dx, dy);
   return (abs(sx-dx) + abs(sy-dy));
}

SInt32
FiniteBufferNetworkModelAtac::computeClusterId(core_id_t core_id)
{
   // Consider a mesh formed by the clusters
   SInt32 cluster_net_width;
   cluster_net_width = _enet_width / _sqrt_cluster_size;

   SInt32 core_x, core_y;
   computeENetPosition(core_id, core_x, core_y);

   SInt32 cluster_pos_x, cluster_pos_y;
   cluster_pos_x = core_x / _sqrt_cluster_size;
   cluster_pos_y = core_y / _sqrt_cluster_size;

   return (cluster_pos_y * cluster_net_width + cluster_pos_x);
}

core_id_t
FiniteBufferNetworkModelAtac::getCoreIdWithOpticalHub(SInt32 cluster_id)
{
   SInt32 cluster_net_width;
   cluster_net_width = _enet_width / _sqrt_cluster_size;

   SInt32 cluster_pos_x, cluster_pos_y;
   cluster_pos_x = cluster_id % cluster_net_width;
   cluster_pos_y = cluster_id / cluster_net_width;

   SInt32 core_x, core_y;
   core_x = cluster_pos_x * _sqrt_cluster_size;
   core_y = cluster_pos_y * _sqrt_cluster_size;

   return (core_y * _enet_width + core_x);
}

void
FiniteBufferNetworkModelAtac::computeCoreIdListInCluster(SInt32 cluster_id, vector<core_id_t>& core_id_list)
{
   SInt32 cluster_net_width;
   cluster_net_width = _enet_width / _sqrt_cluster_size;

   SInt32 cluster_pos_x, cluster_pos_y;
   cluster_pos_x = cluster_id % cluster_net_width;
   cluster_pos_y = cluster_id / cluster_net_width;

   SInt32 optical_hub_x, optical_hub_y; 
   optical_hub_x = cluster_pos_x * _sqrt_cluster_size;
   optical_hub_y = cluster_pos_y * _sqrt_cluster_size;

   for (SInt32 i = optical_hub_x; i < optical_hub_x + _sqrt_cluster_size; i++)
   {
      for (SInt32 j = optical_hub_y; j < optical_hub_y + _sqrt_cluster_size; j++)
      {
         core_id_t core_id = j * _enet_width + i;
         if (core_id < (SInt32) Config::getSingleton()->getTotalCores())
            core_id_list.push_back(core_id);
      }
   }
}

bool
FiniteBufferNetworkModelAtac::isHub(Router::Id router_id)
{
   return ((router_id._index == SENDING_HUB) || (router_id._index == RECEIVING_HUB));
}

SInt32
FiniteBufferNetworkModelAtac::computeSubClusterId(core_id_t core_id)
{
   SInt32 core_x, core_y, access_x, access_y;
   computeENetPosition(core_id, core_x, core_y);
   computeENetPosition(_access_point_list[0]._core_id, access_x, access_y);

   if (core_y <= access_y)
   {
      if (core_x <= access_x)
         return 0;
      else
         return 1;
   }
   else
   {
      if (core_x <= access_x)
         return 2;
      else
         return 3;
   }
}

bool
FiniteBufferNetworkModelAtac::isAccessPoint(Router::Id router_id)
{
   return (find(_access_point_list.begin(), _access_point_list.end(), router_id) != \
         _access_point_list.end());
}

Router::Id&
FiniteBufferNetworkModelAtac::getNearestAccessPoint(core_id_t core_id)
{
   return _access_point_list[computeSubClusterId(core_id)];
}

void
FiniteBufferNetworkModelAtac::outputSummary(ostream& out)
{
   FiniteBufferNetworkModel::outputSummary(out);
}

pair<bool,SInt32>
FiniteBufferNetworkModelAtac::computeCoreCountConstraints(SInt32 core_count)
{
   // This is before 'total_cores' is decided
   SInt32 enet_width = (SInt32) floor (sqrt(core_count));
   SInt32 enet_height = (SInt32) ceil (1.0 * core_count / enet_width);

   assert(core_count <= enet_width * enet_height);
   assert(core_count > (enet_width - 1) * enet_height);
   assert(core_count > enet_width * (enet_height - 1));

   return make_pair(true, enet_height * enet_width);
}

pair<bool,vector<core_id_t> >
FiniteBufferNetworkModelAtac::computeMemoryControllerPositions(SInt32 num_memory_controllers)
{
   // Initialize the topology parameters in case called by an external model
   initializeANetTopologyParameters();
   // _enet_width, _enet_height, _cluster_size, _sqrt_cluster_size, _num_clusters

   LOG_ASSERT_ERROR(num_memory_controllers <= _num_clusters,
         "num_memory_controllers(%i), num_clusters(%i)", num_memory_controllers, _num_clusters);

   vector<core_id_t> core_id_list_with_memory_controllers;
   for (SInt32 i = 0; i < num_memory_controllers; i++)
   {
      core_id_list_with_memory_controllers.push_back(getCoreIdWithOpticalHub(i));
   }

   return (make_pair(true, core_id_list_with_memory_controllers));
}
