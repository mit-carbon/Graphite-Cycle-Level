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
      
      enum GlobalRoutingStrategy
      {
         DISTANCE_BASED = 0,
         CLUSTER_BASED
      };
      enum GlobalRoute
      {
         GLOBAL_ENET = 0,
         GLOBAL_ONET
      };
      enum ReceiveNetType
      {
         HTREE = 0,
         STAR
      };
      enum NodeType
      {
         EMESH = 0,
         SEND_HUB = 1,
         RECEIVE_HUB = 2,
         STAR_NET_ROUTER_BASE = 3
      };

      ////// Private Variables
     
      //// Non-Static
      
      volatile float _frequency;

      // Cluster ID
      SInt32 _cluster_id;
      
      // Buffer Management Scheme
      BufferManagementScheme::Type _buffer_management_scheme;

      //// Static 
      
      static bool _initialized;
      // Tile Width
      static double _tile_width;
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
      static SInt32 _num_receive_nets_per_cluster;
      
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

      // Global Routing Strategy
      static GlobalRoutingStrategy _global_routing_strategy;
      static SInt32 _unicast_distance_threshold;
      // Type of Network on Receiver Cluster
      static ReceiveNetType _receive_net_type;

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
      static double computeOpticalLinkLength();

      ////// Non-Static Private Functions

      //// Utilities
      //// Initialization
      static void initializeANetTopologyParameters();
      static void initializeANetRoutingParameters();
      
      // Creating router object
      NetworkNode* createNetworkNode(SInt32 node_type);

      //// Routing Functions
      // Compute the Global route taken by a packet (ENet / ONet)
      static GlobalRoute computeGlobalRoute(core_id_t sender, core_id_t receiver);
      // BNet link/channel to send the packet on
      static SInt32 computeReceiveNetID(core_id_t sender);
      // Parsing Functions
      static GlobalRoutingStrategy parseGlobalRoutingStrategy(string str);
      static ReceiveNetType parseReceiveNetType(string str);

      // Compute Next Hops on ENet, ONet, (HTree or StarNet)
      void computeNextHopsOnENet(NetworkNode* curr_network_node,
            core_id_t sender, core_id_t receiver,
            vector<Channel::Endpoint>& output_endpoint_vec);
      void computeNextHopsOnONet(NetworkNode* curr_network_node,
            core_id_t sender, core_id_t receiver,
            vector<Channel::Endpoint>& output_endpoint_vec);
      void computeNextHopsOnHTree(NetworkNode* curr_network_node,
            core_id_t sender, core_id_t receiver,
            vector<Channel::Endpoint>& output_endpoint_vec);
      void computeNextHopsOnStarNet(NetworkNode* curr_network_node,
            core_id_t sender, core_id_t receiver,
            vector<Channel::Endpoint>& output_endpoint_vec);
    
      // Event Counters Summary 
      void outputEventCountSummary(ostream& out);

      // Virtual Function in FiniteBufferNetworkModel
      void computeOutputEndpointList(HeadFlit* head_flit, NetworkNode* curr_network_node);
      // Compute Ingress Router Id
      Router::Id computeIngressRouterId(core_id_t core_id);
};
