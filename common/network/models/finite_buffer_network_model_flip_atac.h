#pragma once

#include <stdlib.h>	//need for random function

#include "finite_buffer_network_model.h"

class FiniteBufferNetworkModelFlipAtac : public FiniteBufferNetworkModel
{
   public:
      enum Stage 
      {
         CORE_INTERFACE = -1,	   	//hard-coded
         BCAST_ROUTER,				   //=0
         MUX_ROUTER,				      //=1
         INGRESS_ROUTER,          	//=2
         MIDDLE_ROUTER,		      	//=3
         EGRESS_ROUTER,		      	//=4
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
      NetworkNode* createNetworkNode(core_id_t node_coreID, SInt32 router_index);
	  	
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
      core_id_t _cluster_id;                        // the id of this core's cluster
      vector<core_id_t> cluster_mux_coreID_list;    // list of core_id's of MUX_ROUTERs this core's cluster contains
	  
     // lists of ingress, middle, and egress routers' coreIDs
	  vector<core_id_t> _ingress_coreID_list;
	  vector<core_id_t> _middle_coreID_list;
	  vector<core_id_t> _egress_coreID_list;
	  
	  // the number of nodes associated with this core 
	  SInt32 _num_nodes_on_core; 		
	  
	  // Virtual function in FiniteBufferNetworkModel 
      void computeOutputEndpointList(Flit* head_flit, NetworkNode* curr_network_node);    	  // Main Routing Function
      // Function to compute the first router that the sender core is connected to (in this case, this is the BCAST router)
      Router::Id computeIngressRouterId(core_id_t core_id);             
            
	  // Private variables
      volatile float _frequency;

      // Rand Data Buffer
      drand48_data _rand_data_buffer;

      // Private Functions
      static void readTopologyParams(UInt32& num_router_ports, UInt32& num_in_routers, UInt32& num_mid_routers, UInt32& num_clusters);
      UInt32 getRandNum(UInt32 start, UInt32 end);          // to generate random middle router index
      
      //functions used in createNetworkNode to compute the core_id of connected routers
      core_id_t computeInterfaceCoreID(core_id_t node_coreID, UInt32 i);               
      core_id_t computeEgressInterfaceCoreID(core_id_t node_coreID, UInt32 i);         
      core_id_t computeIngressCoreID(UInt32 i);
      core_id_t computeMiddleCoreID(UInt32 i);
      core_id_t computeEgressCoreID(UInt32 i);
      
};


