#pragma once

#include "finite_buffer_network_model.h"

class FiniteBufferNetworkModelAtac : public FiniteBufferNetworkModel
{
   public:
      FiniteBufferNetworkModelAtac(Network* network, SInt32 network_id);
      ~FiniteBufferNetworkModelAtac();

      volatile float getFrequency() { return _frequency; }
    
      // Output Summary 
      void outputSummary(ostream& out);

      static pair<bool,SInt32> computeCoreCountConstraints(SInt32 core_count);
      static pair<bool,vector<core_id_t> > computeMemoryControllerPositions(SInt32 num_memory_controllers);

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
         EMESH = 0,
         SENDING_HUB = 1,
         RECEIVING_HUB = 2
      };

      ////// Private Variables
     
      //// Static 
      static bool _initialized;
      // Topology Related
      static SInt32 _enet_width;
      static SInt32 _enet_height;
      // Clusters
      static SInt32 _num_clusters;
      static SInt32 _cluster_size;
      static SInt32 _numX_clusters;
      static SInt32 _numY_clusters;
      static SInt32 _cluster_width;
      static SInt32 _cluster_height;
      // Sub-Clusters
      static SInt32 _num_optical_access_points;
      static SInt32 _num_sub_clusters;
      static SInt32 _numX_sub_clusters;
      static SInt32 _numY_sub_clusters;
      static SInt32 _sub_cluster_width;
      static SInt32 _sub_cluster_height;
      // BNets
      static SInt32 _num_bnets_per_cluster;
      
      // Cluster Info
      class ClusterInfo
      {
      public:
         class Boundary
         { 
         public:
            Boundary()
               : minX(0), maxX(0), minY(0), maxY(0) {}
            Boundary(SInt32 minX_, SInt32 maxX_, SInt32 minY_, SInt32 maxY_)
               : minX(minX_), maxX(maxX_), minY(minY_), maxY(maxY_) {}
            ~Boundary() {}
            SInt32 minX, maxX, minY, maxY;
         };
         Boundary _boundary;
         vector<Router::Id> _access_point_list;
      };
      static vector<ClusterInfo> _cluster_info_list;

      //// Non-Static
      volatile float _frequency;

      // Cluster ID
      SInt32 _cluster_id;

      // Global Routing Strategy
      GlobalRoutingStrategy _global_routing_strategy;
      SInt32 _unicast_distance_threshold;
      // Local Route variables
      LocalRoute _local_broadcast_route;
      LocalRoute _local_unicast_route;

      //// Utilities
      static void initializeClusters();
      static void initializeAccessPointList(SInt32 cluster_id);

      static core_id_t computeCoreID(SInt32 x, SInt32 y);
      static void computePosition(core_id_t core_id, SInt32& x, SInt32& y);
      static SInt32 computeDistance(core_id_t sender, core_id_t receiver);
      static void computeCoreIDListInCluster(SInt32 cluster_id, vector<core_id_t>& core_id_list_in_cluster);
      static SInt32 computeClusterID(core_id_t core_id);
      static SInt32 computeSubClusterID(core_id_t core_id);
      static Router::Id computeNearestAccessPoint(core_id_t core_id);
      static bool isAccessPoint(Router::Id router_id);
      static core_id_t computeCoreIDWithOpticalHub(SInt32 cluster_id);
      static bool isHub(Router::Id router_id);

      ////// Non-Static Private Functions

      //// Utilities
      //// Initialization
      static void initializeANetTopologyParameters();
      
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
      void computeNextHopsOnENet(NetworkNode* curr_network_node,
            core_id_t sender, core_id_t receiver,
            vector<Channel::Endpoint>& output_endpoint_list);
      void computeNextHopsOnONet(NetworkNode* curr_network_node,
            core_id_t sender, core_id_t receiver,
            vector<Channel::Endpoint>& output_endpoint_list);
      void computeNextHopsOnBNet(NetworkNode* curr_network_node,
            core_id_t sender, core_id_t receiver,
            vector<Channel::Endpoint>& output_endpoint_list);
      
      // Virtual Function in FiniteBufferNetworkModel
      void computeOutputEndpointList(Flit* head_flit, NetworkNode* curr_network_node);
      // Compute Ingress Router Id
      Router::Id computeIngressRouterId(core_id_t core_id);
};
