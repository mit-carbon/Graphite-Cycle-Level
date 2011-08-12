#pragma once

#include <stdlib.h>

#include "finite_buffer_network_model.h"

class FiniteBufferNetworkModelClos : public FiniteBufferNetworkModel
{
   public:
      enum Stage 
      {
         CORE_INTERFACE = -1,	     // hard-coded
         INGRESS_ROUTER,           //=0
         MIDDLE_ROUTER,		        //=1
         EGRESS_ROUTER,		        //=2
      };

      // Initialize network topology & create network nodes for this core
      // This constructor is called for each core in network
      FiniteBufferNetworkModelClos(Network* network, SInt32 network_id);
      ~FiniteBufferNetworkModelClos();

      volatile float getFrequency() { return _frequency; }
      void outputSummary(ostream& out);            // prints to output_files/sim.out
      void reset() {}
     
      static pair<bool,UInt32> computeCoreCountConstraints(SInt32 core_count); 
      static pair<bool,vector<core_id_t> > computeMemoryControllerPositions(SInt32 num_memory_controllers);
      
      
   private:
      
      // Create a network node (a router with appropriate connections and performance/power models)
      NetworkNode* createNetworkNode(core_id_t node_coreID, SInt32 router_index);
	  	
	   // Topology Parameters
      // m x n x r Clos network (according to Dally notation)
      UInt32 num_mid_routers;			   // m, the number of middle routers
      UInt32 num_router_ports;			// n, the number of input (or output) ports to each ingress (or egress) router
      UInt32 num_in_routers;			   // r, the number of ingress (or egress) routers	
	  
      
      // Lists of ingress, middle, and egress routers's coreIDs
      // Every core has every type of network node, but only certain nodes ("real nodes") will actually be used in the network
      // If a core has a "real" network node, its ID is put into the corresponding list
      vector<core_id_t> ingress_coreID_list;
      vector<core_id_t> middle_coreID_list;
      vector<core_id_t> egress_coreID_list;

      UInt32 _num_flits;
      
      // Virtual function in FiniteBufferNetworkModel
      // Main Routing Function ************************
      // Compute the next router to send the head packet to
      void computeOutputEndpointList(Flit* head_flit, NetworkNode* curr_network_node);
      
      // Compute which ingress router the sender core is connected to
      Router::Id computeIngressRouterId(core_id_t core_id);
            
      // Private variables
      volatile float _frequency;

      // Rand Data Buffer
      drand48_data _rand_data_buffer;

      // Private Functions
      static void readTopologyParams(UInt32& m, UInt32& n, UInt32& r);
      UInt32 getRandNum(UInt32 start, UInt32 end);          // Generate random middle router index
      void outputEventCountSummary(ostream& out);           // Print out event counts in output_files/sim.out file

      //functions used in createNetworkNode to compute the core_id of connected routers
      core_id_t computeInterfaceCoreID(core_id_t node_coreID, UInt32 i);
      core_id_t computeEgressInterfaceCoreID(core_id_t node_coreID, UInt32 i);
      core_id_t computeIngressCoreID(core_id_t node_coreID, UInt32 i);
      core_id_t computeMiddleCoreID(core_id_t node_coreID, UInt32 i);
      core_id_t computeEgressCoreID(core_id_t node_coreID, UInt32 i);
      
};


