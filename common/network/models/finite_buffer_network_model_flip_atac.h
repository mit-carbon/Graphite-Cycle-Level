#pragma once

#include <stdlib.h>

#include "finite_buffer_network_model.h"

class FiniteBufferNetworkModelFlipAtac : public FiniteBufferNetworkModel
{
   public:
      enum Stage 
      {
         BCAST_ROUTER = 1,    // Always starts at 1
         MUX_ROUTER = 2,
         INGRESS_ROUTER = 3,
         MIDDLE_ROUTER = 4,
         EGRESS_ROUTER = 5
      };

      // Constructor initializes network topology & creates network nodes for this core
      // Note: each core has all types of network nodes, but the MUX, INGRESS, MIDDLE, and EGRESS routers 
      // are only used if the core's id appears in the associated list (ex: _ingress_coreID_list) see below
      FiniteBufferNetworkModelFlipAtac(Network* network, SInt32 network_id);
      ~FiniteBufferNetworkModelFlipAtac();

      volatile float getFrequency() { return _frequency; }
      void outputSummary(ostream& out);
      void reset() {}
     
      static pair<bool,UInt32> computeCoreCountConstraints(SInt32 core_count); 
      static pair<bool,vector<core_id_t> > computeMemoryControllerPositions(SInt32 num_memory_controllers);
      
      
   private:
      
      // Creates a network node (a router with appropriate connections and performance/power models)
      NetworkNode* createNetworkNode(core_id_t node_coreID, SInt32 router_index, core_id_t mid_cluster_id);

      // Topology Parameters
      // m x n x r Clos network (according to Dally notation)
      UInt32 _num_mid_routers;			// m, the number of middle routers
      UInt32 _num_router_ports;			// n, the number of input (or output) ports to each input (or output) router
      UInt32 _num_in_routers;			   // r, the number of input/output routers	
      UInt32 _num_clusters;
      // Other useful topology parameters that are derived from above user-input parameters
      UInt32 _num_cores; 				   // = num_in_routers * num_router_ports
      UInt32 _num_cores_per_cluster; 	// = num_cores/num_clusters

      // Info about the cluster this core is located on
      core_id_t _cluster_id;                       // the id of this core's cluster
      core_id_t _mid_cluster_id;					      // need middle router's cluster id for cases when _num_mid_routers > _num_in_routers since middle router with this coreID might be in different cluster than core with this ID
      vector<core_id_t> cluster_mux_coreID_list;    // list of MUX_ROUTER coreIDs this cluster contains

      // lists of ingress, middle, and egress routers' coreIDs
      vector<core_id_t> _ingress_coreID_list;
      vector<core_id_t> _middle_coreID_list;
      vector<core_id_t> _egress_coreID_list;

      // Virtual function in FiniteBufferNetworkModel 
      // Main Routing Function ************************
      // Compute the next router to send the head packet to
      void computeOutputEndpointList(HeadFlit* head_flit, NetworkNode* curr_network_node);    	  
      // Function to compute the first router that the sender core is connected to (for flip_atac, this is the BCAST router)
      Router::Id computeIngressRouterId(core_id_t core_id);             
            
      // Private variables
      volatile float _frequency;

      // Rand Data Buffer
      drand48_data _rand_data_buffer;

      // Private Functions
      static void readTopologyParams(UInt32& num_router_ports, UInt32& num_in_routers, UInt32& num_mid_routers, UInt32& num_clusters);
      UInt32 getRandNum(UInt32 start, UInt32 end);          // to generate random middle router index
      void outputEventCountSummary(ostream& out);           // Print out event counts in sim.out file

      //functions used in createNetworkNode to compute the core_id of connected routers
      core_id_t computeInterfaceCoreID(core_id_t node_coreID, UInt32 i);               
      core_id_t computeEgressInterfaceCoreID(core_id_t node_coreID, UInt32 i);         
      core_id_t computeIngressCoreID(UInt32 i);
      core_id_t computeMiddleCoreID(UInt32 i);
      core_id_t computeEgressCoreID(UInt32 i);
      
};


