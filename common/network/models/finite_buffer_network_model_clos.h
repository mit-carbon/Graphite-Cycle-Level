#pragma once

#include <stdlib.h>	//need for random function

#include "finite_buffer_network_model.h"

class FiniteBufferNetworkModelClos : public FiniteBufferNetworkModel
{
   public:
      enum Stage 
      {
         CORE_INTERFACE = -1,	   //hard-coded
         INGRESS_ROUTER,          //=0
         MIDDLE_ROUTER,		      //=1
         EGRESS_ROUTER,		      //=2
      };

      // Initializes network topology & creates network node(s) for this core: sending, receiving, and possible ingress, middle, egress nodes
      FiniteBufferNetworkModelClos(Network* network, SInt32 network_id);
      ~FiniteBufferNetworkModelClos();

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
      UInt32 num_mid_routers;			// m, the number of middle routers
      UInt32 num_router_ports;			// n, the number of input (or output) ports to each input (or output) router
      UInt32 num_in_routers;			// r, the number of input/output routers	
	  
      
	  //lists of ingress, middle, and egress routers's coreIDs
	  //construct this list in the Clos constructor 
	  //use this list before calling createNetworkNode, 
	  //in order to determine if for this coreID, you need to create ingress, middle, and/or egress routers
	  //note: all cores will create sending and receiving routers
	  vector<core_id_t> ingress_coreID_list;
	  vector<core_id_t> middle_coreID_list;
	  vector<core_id_t> egress_coreID_list;
	  
	  //the number of nodes associated with this core 
	  //min = 2: SENDING_CORE, RECEIVING_CORE but could also have INGRESS, MIDDLE, EGRESS nodes (on one core, can only have one of each though)
	  //max = 5
	  SInt32 num_nodes_on_core; 		
	  
	  // // Event counters
     // // note: router_switch_allocator gets incremented once per packet
     // // and the rest of event counters get incremented once per flit
     // UInt64 _input_link_traversal;                  //from core to INGRESS
     // UInt64 _ingress_to_mid_link_traversal;         //from INGRESS to MIDDLE
     // UInt64 _mid_to_egress_link_traversal;          //from MIDDLE to EGRESS
     // UInt64 _output_link_traversal;                 //from EGRESS to core
     // // INGRESS router event counts
     // UInt64 _ingress_buffer_access;                
     // UInt64 _ingress_switch_allocator;              
     // UInt64 _ingress_crossbar_traversal;
     // // MIDDLE router event counts
     // UInt64 _mid_buffer_access;                
     // UInt64 _mid_switch_allocator;              
     // UInt64 _mid_crossbar_traversal;
     // // EGRESS router event counts
     // UInt64 _egress_buffer_access;                
     // UInt64 _egress_switch_allocator;              
     // UInt64 _egress_crossbar_traversal;
     void outputEventCountSummary(ostream& out);
     
     UInt32 _num_flits;
     // Virtual function in FiniteBufferNetworkModel
	  // Main Routing Function ************************
      void computeOutputEndpointList(Flit* head_flit, NetworkNode* curr_network_node);
      // Function to compute ingress router sender core is connected to
      Router::Id computeIngressRouterId(core_id_t core_id);
            
	  // Private variables
      volatile float _frequency;

      // Rand Data Buffer
      drand48_data _rand_data_buffer;

      // Private Functions
      static void readTopologyParams(UInt32& m, UInt32& n, UInt32& r);
      UInt32 getRandNum(UInt32 start, UInt32 end);          // to generate random middle router index
      
      //functions used in createNetworkNode to compute the core_id of connected routers
      core_id_t computeInterfaceCoreID(core_id_t node_coreID, UInt32 i);
      core_id_t computeEgressInterfaceCoreID(core_id_t node_coreID, UInt32 i);
      core_id_t computeIngressCoreID(core_id_t node_coreID, UInt32 i);
      core_id_t computeMiddleCoreID(core_id_t node_coreID, UInt32 i);
      core_id_t computeEgressCoreID(core_id_t node_coreID, UInt32 i);
      
};


