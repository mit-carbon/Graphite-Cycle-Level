#pragma once

#include "finite_buffer_network_model.h"

class FiniteBufferNetworkModelAtac : public FiniteBufferNetworkModel
{
   public:
      FiniteBufferNetworkModelAtac(Network* network, SInt32 network_id);
      ~FiniteBufferNetworkModelAtac();

      volatile float getFrequency() { return _frequency; }
    
      // Output Summary 
      void outputSummary(ostream& out) { /* FIXME: Fill me in */ }

   private:
      ////// Private Enumerators
      
      enum GlobalRoute
      {
         GLOBAL_ENET = 0,
         GLOBAL_ONET,
         NUM_GLOBAL_ROUTES
      };
      enum LocalRoute
      {
         LOCAL_ENET = 0,
         LOCAL_BNET,
         NUM_LOCAL_ROUTES
      };
      enum GlobalRoutingStrategy
      {
         DISTANCE_BASED = 0,
         CLUSTER_BASED,
         NUM_GLOBAL_ROUTING_STRATEGIES
      };
      enum NodeType
      {
         CORE_INTERFACE = -1,
         EMESH = 0,
         SENDING_HUB = 1,
         RECEIVING_HUB = 2
      };

      ////// Private Variables
     
      //// Static 
      // Topology Related
      static SInt32 _enet_width;
      static SInt32 _enet_height;
      static SInt32 _num_clusters;
      static SInt32 _cluster_size;
      static SInt32 _sqrt_cluster_size;
      
      //// Non-Static
      volatile float _frequency;

      SInt32 _num_bnets_in_cluster;

      // Cluster Id on which this core is located
      core_id_t _cluster_id;
      // Routers on which the access points for this cluster are located
      vector<Router::Id> _access_point_list;

      // Global Routing Strategy
      GlobalRoutingStrategy _global_routing_strategy;
      SInt32 _unicast_distance_threshold;

      // Local Route variables
      LocalRoute _local_broadcast_route;
      LocalRoute _local_unicast_route;

      // FIXME: Remove some of these to (non-static)

      //// Utilities
      static core_id_t computeCoreId(SInt32 x, SInt32 y);
      static void computeENetPosition(core_id_t core_id, SInt32& x, SInt32& y);
      static SInt32 computeDistanceOnENet(core_id_t sender, core_id_t receiver);
      static SInt32 computeClusterId(core_id_t core_id);
      static core_id_t getCoreIdWithOpticalHub(SInt32 cluster_id);
      static void computeCoreIdListInCluster(SInt32 cluster_id, vector<core_id_t>& core_id_list_in_cluster);
      static bool isHub(Router::Id router_id);

      ////// Non-Static Private Functions

      //// Utilities
      // Get Sub Cluster Id
      SInt32 computeSubClusterId(core_id_t core_id);
      // Is the router an Access Point?
      bool isAccessPoint(Router::Id router_id);
      // Nearest Access Point
      Router::Id& getNearestAccessPoint(core_id_t core_id);
      // Cluster Width, Cluster Height
      SInt32 computeClusterWidth();
      SInt32 computeClusterHeight();

      //// Initialization
      void initializeANetTopologyParameters();
      // Computing access points
      void createAccessPointList();
      // Creating router object
      NetworkNode* createNetworkNode(NodeType node_type);

      //// Routing Functions
      // Compute the Global route taken by a packet (ENet / ONet)
      GlobalRoute computeGlobalRoute(core_id_t sender, core_id_t receiver);
      // Compute the Local route taken by a packet at its receiving cluster (ENet / BNet)
      LocalRoute computeLocalRoute(core_id_t receiver);
      // BNet link/channel to send the packet on
      SInt32 getBNetChannelId(core_id_t sender);
      // Parsing Functions
      GlobalRoutingStrategy parseGlobalRoutingStrategy(string str);
      LocalRoute parseLocalRoute(string str);

      // Compute Next Hops
      void computeNextHopsOnENet(NetworkNode* curr_network_node, \
            core_id_t sender, core_id_t receiver, \
            vector<Channel::Endpoint>& output_endpoint_list);
      void computeNextHopsOnONet(NetworkNode* curr_network_node, \
            core_id_t sender, core_id_t receiver, \
            vector<Channel::Endpoint>& output_endpoint_list);
      void computeNextHopsOnBNet(NetworkNode* curr_network_node, \
            core_id_t sender, core_id_t receiver, \
            vector<Channel::Endpoint>& output_endpoint_list);
      
      // Virtual Function in FiniteBufferNetworkModel
      void computeOutputEndpointList(Flit* head_flit, NetworkNode* curr_network_node);
      UInt64 computeUnloadedDelay(core_id_t sender, core_id_t receiver, SInt32 num_flits)
      { return 0; /* FIXME: Fill me in */ }
};
