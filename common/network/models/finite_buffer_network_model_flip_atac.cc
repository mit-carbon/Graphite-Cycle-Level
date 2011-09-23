#include <algorithm> 
#include <cmath>	

#include "router.h"
#include "router_performance_model.h"
#include "router_power_model.h"
#include "electrical_link_performance_model.h"
#include "electrical_link_power_model.h"
#include "finite_buffer_network_model_flip_atac.h"
#include "simulator.h"
#include "config.h"
#include "core.h"   
#include "log.h"

FiniteBufferNetworkModelFlipAtac::FiniteBufferNetworkModelFlipAtac(Network* network, SInt32 network_id):
   FiniteBufferNetworkModel(network, network_id)
{
   LOG_PRINT("Enter FiniteBufferNetworkModelFlipAtac with core_id %i", getNetwork()->getCore()->getId());
    
   // Initialize Clos Topology Parameters from user input in config file
   readTopologyParams(_num_router_ports, _num_in_routers, _num_mid_routers, _num_clusters);
   LOG_PRINT("num_router ports = %u, _num_in_routers = %u, _num_mid_routers = %u, num_clusters = %u", _num_router_ports, _num_in_routers, _num_mid_routers, _num_clusters);
   
   // Calculate rest of topology parameters                      
   _num_cores_per_cluster = _num_in_routers * _num_router_ports;     // note: each cluster has its own Clos
   _num_cores = _num_cores_per_cluster * _num_clusters;
   
   // Router, Link Params
   
   // Get Network Parameters
   try
   {
      _frequency = Sim()->getCfg()->getFloat("network/flip_atac/frequency");
      _flit_width = Sim()->getCfg()->getInt("network/flip_atac/flit_width");    //variable inherited from finite_buffer_network_model
      _flow_control_scheme = FlowControlScheme::parse(Sim()->getCfg()->getString("network/flip_atac/flow_control_scheme")); //variable inherited from finite buff model
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Unable to read Clos network parameters from cfg file");
   }

   // Each core has one of each type of node (router)
   // The BCAST router on each core is always used
   // But each MUX, INGRESS, MIDDLE, and EGRESS is only used if the core's id is found in the following lists
   
   // Note: each core has ingress, middle, egress lists for its cluster only
   // Get this core's id and cluster id
   core_id_t core_id = getNetwork()->getCore()->getId();		
   _cluster_id = core_id/_num_cores_per_cluster;
   
   // determine which cores in this cluster have ingress, middle and/or egress routers mapped to them
	// store these coreIDs in list for ingress, middle and/or egress
	LOG_PRINT("Computing which cores have ingress, middle, and/or egress routers...");
	// INGRESS_ROUTER list of coreIDs
   for (UInt32 i = 0; i < (_num_in_routers *_num_clusters); i++)
   {
      _ingress_coreID_list.push_back(i * _num_router_ports);		//this is the mapping convention for ingress routers
   }
	
	// MIDDLE_ROUTER list of coreIDs
   SInt32 num_in_cores = _num_in_routers * _num_router_ports;
   for (UInt32 i = 0; i < (_num_mid_routers *_num_clusters); i++)
   {
      _middle_coreID_list.push_back( (i * (num_in_cores/_num_mid_routers)) + 1);
   }

    _mid_cluster_id = _cluster_id;
	// need middle router cluster id for cases when _num_mid_routers > _num_in_routers
    if (core_id){ //if core_id = 0, keep _mid_cluster_id 0
		UInt32 mid_index = ((core_id -1) / (num_in_cores/_num_mid_routers)); 
		_mid_cluster_id = mid_index/_num_mid_routers;
	}

	LOG_PRINT("Egress router list for cluster id %i", _cluster_id);
	// EGRESS_ROUTER list of coreIDs
   for (UInt32 i = 0; i < (_num_in_routers*_num_clusters); i++)
   {
      _egress_coreID_list.push_back( (i * _num_router_ports) + _num_router_ports-1);
   }
   
   
   // Create this cluster's list of MUX_ROUTERs
   for (UInt32 i = 0; i < _num_cores_per_cluster; i++){
      core_id_t mux_core_id = (_cluster_id * _num_cores_per_cluster) + i;
      cluster_mux_coreID_list.push_back(mux_core_id);
   }
  
   // Net Packet Injector
   _network_node_map[NET_PACKET_INJECTOR] = createNetPacketInjectorNode(computeIngressRouterId(_core_id),
         BufferManagementScheme::parse(Sim()->getCfg()->getString("network/flip_atac/buffer_management_scheme")),
         Sim()->getCfg()->getInt("network/flip_atac/router/input_buffer_size"));
         
   // Always create all types of nodes (BCAST, MUX, INGRESS, MIDDLE, EGRESS) for each core. 
   // Note: this is to comply with the finite buffer model framework (indexing in _network_node_map vector)
   LOG_PRINT("Create BCAST_ROUTER for core_id %i", core_id);
	NetworkNode* bcast_router = createNetworkNode(core_id, BCAST_ROUTER, _mid_cluster_id);
	// add this node to the list of network nodes associated with this core (list is declared protected in "finite_buffer_network_model.h")
	_network_node_map[BCAST_ROUTER] = bcast_router;
   
   LOG_PRINT("Create MUX_ROUTER for core_id %i", core_id);
	NetworkNode* mux_router = createNetworkNode(core_id, MUX_ROUTER, _mid_cluster_id);
	_network_node_map[MUX_ROUTER] = mux_router;          // add this node to the list of network nodes associated with this core
   
   LOG_PRINT("Create INGRESS_ROUTER for core_id %i", core_id);
	NetworkNode* ingress_router = createNetworkNode(core_id, INGRESS_ROUTER, _mid_cluster_id);
	_network_node_map[INGRESS_ROUTER] = ingress_router;      // add this node to the list of network nodes associated with this core
         
   LOG_PRINT("Create MIDDLE_ROUTER for core_id %i", core_id);
	NetworkNode* middle_router = createNetworkNode(core_id, MIDDLE_ROUTER, _mid_cluster_id);
	_network_node_map[MIDDLE_ROUTER] = middle_router;       // add this node to the list of network nodes associated with this core
	
   LOG_PRINT("Create EGRESS_ROUTER for core_id %i", core_id);
	NetworkNode* egress_router = createNetworkNode(core_id, EGRESS_ROUTER, _mid_cluster_id);
	_network_node_map[EGRESS_ROUTER] = egress_router;       // add this node to the list of network nodes associated with this core
   
   // Seed the buffer for random number generation --> will be used for MIDDLE_ROUTER stage in Clos
   srand48_r(core_id, &_rand_data_buffer);
   
   LOG_PRINT("Exit FiniteBufferNetworkModelFlipAtac constructor core_id %i", core_id);
}

FiniteBufferNetworkModelFlipAtac::~FiniteBufferNetworkModelFlipAtac()
{
   map<SInt32, NetworkNode*>::iterator it = _network_node_map.begin();
   for ( ; it != _network_node_map.end(); it ++)
      delete (*it).second;
}


void
FiniteBufferNetworkModelFlipAtac::readTopologyParams(UInt32& num_router_ports, UInt32& num_in_routers, UInt32& num_mid_routers, UInt32& num_clusters)
{
   try
   {
      num_router_ports = Sim()->getCfg()->getInt("network/flip_atac/num_router_ports");
      num_in_routers = Sim()->getCfg()->getInt("network/flip_atac/num_in_routers");
      num_mid_routers = Sim()->getCfg()->getInt("network/flip_atac/num_mid_routers");
      num_clusters = Sim()->getCfg()->getInt("network/flip_atac/num_clusters");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read FLIP ATAC Topology Params from cfg file");
   }
}


// Function makes ChannelEndpointList in head flit object which contains all the next destinations of the flit
// This is the main routing function.
void
FiniteBufferNetworkModelFlipAtac::computeOutputEndpointList(HeadFlit* head_flit, NetworkNode* curr_network_node)
{
   LOG_PRINT("computeOutputEndpointList: head_flit, curr_network _node =(%p,%p) enter", head_flit, curr_network_node);
   LOG_PRINT("head_flit->_sender %i, head_flit->_receiver %i", head_flit->_sender, head_flit->_receiver);
   
   // get the current router id
   Router::Id curr_router_id = curr_network_node->getRouterId();
   core_id_t curr_core_id = curr_router_id._core_id;
   SInt32 curr_router_index = curr_router_id._index;
   LOG_PRINT("Current Router Id [%i,%i])", curr_router_id._core_id, curr_router_id._index);
   
   assert(_core_id == curr_router_id._core_id);
   
   list<Router::Id> next_dest_list;                   // list of next destination routerIDs
   vector<Channel::Endpoint> output_endpoint_vec;     // list of endpoints to go to

   if (curr_router_index == BCAST_ROUTER){
      LOG_PRINT("At BCAST_ROUTER with core_id %i. Compute next destination.", curr_core_id);
      
      // next destination is broadcast to every cluster (all output endpoints)
      
      // in this version, do not actually broadcast to all clusters
      // just go to cluster that contains the receiving core
      // this still models broadcast performance & power because there is only one output channel (with endpoints to each cluster)
      // this channel is considered utilized whether you send to one endpoint (one cluster) or all endpoints (all clusters)
            
      //find cluster ID of receiving core
      core_id_t receiving_clusterID = (head_flit->_receiver) / _num_cores_per_cluster;
      
      core_id_t mux_coreID = receiving_clusterID * _num_cores_per_cluster + (curr_core_id / _num_clusters); //mux_index =(node_coreID/_num_clusters);
      Router::Id router_id(mux_coreID, MUX_ROUTER);
      next_dest_list.push_back(router_id);
      LOG_PRINT("Next destination Router Id [%i,%i]", router_id._core_id, router_id._index);
         
      Channel::Endpoint& output_endpoint = curr_network_node->getOutputEndpointFromRouterId(router_id); // get endpoint from router id
      output_endpoint_vec.push_back(output_endpoint);      
      
      LOG_PRINT("Next Router(%i,%i), Output Endpoint(%i,%i)", router_id._core_id, router_id._index, output_endpoint._channel_id, output_endpoint._index);
      
   }
   
   else if (curr_router_index == MUX_ROUTER){
      LOG_PRINT("At MUX_ROUTER with core_id %i. Compute next destination.", curr_core_id);
      // next destination is ingress router of Clos
      
      // check if receiver core is in this cluster-->so can drop packets if need to
      // actually, don't need to drop packets in this version of code, but keep this in as an error check
      bool core_in_cluster = false;
      for (UInt32 i = 0; i < cluster_mux_coreID_list.size(); i++){
         if (head_flit->_receiver == cluster_mux_coreID_list[i]){
             // receiver core is in this cluster
             core_in_cluster = true;
         }
      }
      
      
      if (!core_in_cluster){ //if receiver core not in this cluster  --> should never be the case in this version of the code, since only send packet to receiving cluster
         // drop the packet!!!
         LOG_PRINT("Drop packet at MUX_ROUTER with core id %i because receiver core %i is not in this cluster %i.", curr_core_id, head_flit->_receiver, _cluster_id);
         head_flit->_output_endpoint_list = new vector<Channel::Endpoint>(vector<Channel::Endpoint>());
         return; 
      }
      
      else { // if receiver core in this cluster 
         // send packet to the ingress router
         core_id_t ingress_coreID = curr_core_id / _num_router_ports * _num_router_ports;         // this is equivalent to computeIngressRouterId() in clos network
         Router::Id router_id(ingress_coreID, INGRESS_ROUTER);
         next_dest_list.push_back(router_id);
         LOG_PRINT("Next destination Router Id [%i,%i]", router_id._core_id, router_id._index);
      
         // add corresponding channel endpoint to the vector (will later be added to the head flit object)
         Channel::Endpoint& output_endpoint = curr_network_node->getOutputEndpointFromRouterId(router_id);    
         output_endpoint_vec.push_back(output_endpoint);
      }
   }
   
   else if (curr_router_index == INGRESS_ROUTER){
      LOG_PRINT("At INGRESS_ROUTER with core_id %i. Compute next destination.", curr_core_id);
      // next destination is a random middle router *in same cluster*
      UInt32 middle_router_idx = getRandNum(0, _num_mid_routers);                 // get random index
      if (_cluster_id)                                                            // leave this index unchanged if _cluster_id = 0
      {      
         middle_router_idx = _cluster_id *_num_mid_routers + middle_router_idx;                    // get middle index at this cluster
      }                   
      UInt32 N = _num_in_routers * _num_router_ports;
      core_id_t next_coreID = (middle_router_idx * (N/_num_mid_routers)) + 1;     // compute coreID of this random middle router from index
      
      Router::Id router_id(next_coreID, MIDDLE_ROUTER); // getRouterIndexFromCoreId(next_coreID, MIDDLE_ROUTER));
      // add next router to the list
      next_dest_list.push_back(router_id);
      LOG_PRINT("Next destination Router Id [%i,%i]", router_id._core_id, router_id._index);
      
      // add corresponding channel endpoint to the vector (will later be added to the head flit object)
      Channel::Endpoint& output_endpoint = curr_network_node->getOutputEndpointFromRouterId(router_id);        // check that the routerID in param of function is supposed to be *next* (not curr) router!!!
      output_endpoint_vec.push_back(output_endpoint);
   }
	
   // if at MIDDLE_ROUTER
   else if (curr_router_index == MIDDLE_ROUTER) {
      LOG_PRINT("At MIDDLE_ROUTER with core_id %i. Compute next destination.", curr_core_id);
      // next destination is the correct egress router
      
      // compute coreID of egress router the receiving core is connected to (based on coreID)
      core_id_t next_coreID = _egress_coreID_list[(head_flit->_receiver/_num_router_ports)]; 
     
	   // this is the router id for next destination
      Router::Id router_id(next_coreID, EGRESS_ROUTER); 
      LOG_PRINT("Next destination Router Id [%i,%i]", router_id._core_id, router_id._index);
      
      // add it to the list
      next_dest_list.push_back(router_id);
      
      // add corresponding channel endpoint to the vector (will later be added to the head flit object)
      Channel::Endpoint& output_endpoint = curr_network_node->getOutputEndpointFromRouterId(router_id);
      output_endpoint_vec.push_back(output_endpoint);
   }
   
   // if at EGRESS_ROUTER
   else if (curr_router_index == EGRESS_ROUTER) {  
      LOG_PRINT("At EGRESS_ROUTER with core_id %i. Compute next destination.", curr_core_id);
      // next destination is the correct receiving core
      core_id_t next_coreID = head_flit->_receiver;
      
      // this is the router id for next destination
      Router::Id  router_id(next_coreID, CORE_INTERFACE);
      LOG_PRINT("Next destination Router Id [%i,%i]", router_id._core_id, router_id._index);
       // add it to the list
      next_dest_list.push_back(router_id);
      
      // add corresponding channel endpoint to the vector (will later be added to the head flit object)
      Channel::Endpoint& output_endpoint = curr_network_node->getOutputEndpointFromRouterId(router_id); 
      output_endpoint_vec.push_back(output_endpoint);
   }
     
   LOG_PRINT("Initialize head flit's channel endpoint list...");
   // In all cases...
   // Initialize the output channel struct inside head_flit
   head_flit->_output_endpoint_list = new vector<Channel::Endpoint>(output_endpoint_vec);
   
   LOG_PRINT("computeOutputEndpointList(%p, %p) end", head_flit, curr_network_node);
}

// creates network node of type router_index for the core with id node_coreID
NetworkNode*
FiniteBufferNetworkModelFlipAtac::createNetworkNode(core_id_t node_coreID, SInt32 router_index, core_id_t mid_cluster_id)
{
	LOG_PRINT("createNetworkNode for Router Id(%i, %i) enter", node_coreID, router_index);
   
   // Read necessary parameters from cfg file
   string buffer_management_scheme_str;
   SInt32 data_pipeline_delay = 0;
   SInt32 credit_pipeline_delay = 0;
   SInt32 router_input_buffer_size = 0;
   string link_type;
   volatile double link_length = 0.0;

   try
   {
      buffer_management_scheme_str = Sim()->getCfg()->getString("network/flip_atac/buffer_management_scheme"); 
      data_pipeline_delay = Sim()->getCfg()->getInt("network/flip_atac/router/data_pipeline_delay"); 
      credit_pipeline_delay = Sim()->getCfg()->getInt("network/flip_atac/router/credit_pipeline_delay"); 
      router_input_buffer_size = Sim()->getCfg()->getInt("network/flip_atac/router/input_buffer_size");
      link_type = Sim()->getCfg()->getString("network/flip_atac/link/type"); 
      link_length = Sim()->getCfg()->getFloat("network/flip_atac/link/length"); 
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read Clos parameters from the cfg file");
   }
     
   BufferManagementScheme::Type buffer_management_scheme = \
         BufferManagementScheme::parse(buffer_management_scheme_str);
   
   // Initialize vectors which will provide router and core connection info
   vector<vector<Router::Id> > input_channel_to_router_id_list__mapping;	//list of routers each input of this network node is connected to
   vector<vector<Router::Id> > output_channel_to_router_id_list__mapping;	//list of routers each output of this network node is connected to
   vector<SInt32> num_input_endpoints_list;									      //list of number of routers each input is connected to 
   vector<SInt32> num_output_endpoints_list;								         //list of number of routers each output is connected to
   vector<BufferManagementScheme::Type> input_buffer_management_schemes;	//buffer management schemes for each input channel 
   vector<BufferManagementScheme::Type> downstream_buffer_management_schemes;//buffer management schemes for endpoint of each output channel
   vector<SInt32> input_buffer_size_list;									         //buffer sizes at each input channel
   vector<SInt32> downstream_buffer_size_list;								      //buffer sizes at endpoint of each output channel

   LOG_PRINT("Creating input and output channels of the network node...");

   // Add input and output connections to the network node
   
   if (router_index == BCAST_ROUTER)
   {
      // BCAST INPUT
      LOG_PRINT("Creating BCAST_ROUTER input...");
      Router::Id net_packet_injector_node(node_coreID, NET_PACKET_INJECTOR);		   // get the id of the input router
		NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, net_packet_injector_node); //add this router to list of node input connections
		num_input_endpoints_list.push_back(1);					            //each channel has one input
			
		 input_buffer_management_schemes.push_back(buffer_management_scheme);
		 input_buffer_size_list.push_back(router_input_buffer_size);
          
      // BCAST OUTPUT
      // In this version, BCAST has one output channel with endpoints at each cluster
      vector<Router::Id> router_output_list;
      for (UInt32 i = 0; i < _num_clusters; i++)
      {
         LOG_PRINT("Creating BCAST_ROUTER output...");
         core_id_t mux_coreID = i * _num_cores_per_cluster + (node_coreID / _num_clusters); // note: mux_index =(node_coreID/_num_clusters);
         Router::Id mux_router(mux_coreID, MUX_ROUTER);
         // create the output endpoint list vector
         router_output_list.push_back(mux_router);
      }
      
      NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, router_output_list); 
		num_output_endpoints_list.push_back(_num_clusters);
         
   	// set buffer management scheme according to config file
		downstream_buffer_management_schemes.push_back(buffer_management_scheme);
		downstream_buffer_size_list.push_back(router_input_buffer_size);
   }
   
   else if (router_index == MUX_ROUTER)
   {
      // MUX INPUT
      // mux has as many input channels as there are clusters in the network
      LOG_PRINT("Creating MUX_ROUTER input...");
      for (UInt32 i = 0; i < _num_clusters; i ++)
      {
         SInt32 mux_index = node_coreID - (_cluster_id * _num_cores_per_cluster);  // index of this mux in this cluster's list
         core_id_t bcast_coreID = (mux_index * _num_clusters) + i;
         Router::Id bcast_router(bcast_coreID, BCAST_ROUTER);
         NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, bcast_router);   // add this core to list of node input connections
         num_input_endpoints_list.push_back(1);					                                       // each channel has one input
		
         // set buffer schemes
			input_buffer_management_schemes.push_back(buffer_management_scheme);
			input_buffer_size_list.push_back(router_input_buffer_size);
      }
      
      // MUX OUTPUT
      LOG_PRINT("Creating MUX_ROUTER output...");
      // compute the coreID of ingress router to connect to
      core_id_t ingress_coreID = node_coreID / _num_router_ports * _num_router_ports;   // this is equivalent to computeIngressRouterId() in clos network
      Router::Id ingress_router(ingress_coreID, INGRESS_ROUTER);
      NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, ingress_router);   // add this core to list of node input connections
      num_output_endpoints_list.push_back(1);					                                       // each channel has one input
		
      // set buffer management scheme according to config file
		downstream_buffer_management_schemes.push_back(buffer_management_scheme);
		downstream_buffer_size_list.push_back(router_input_buffer_size);
   }
   
   else if (router_index == INGRESS_ROUTER)
   {
      // INGRESS INPUT 
	   LOG_PRINT("Creating INGRESS input...");
      // for each input channel of ingress router
		for (UInt32 i = 0; i < _num_router_ports; i++)
      {
			core_id_t coreID = computeInterfaceCoreID(node_coreID, i);
			Router::Id mux_router(coreID, MUX_ROUTER);		
			NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, mux_router); // add this core to list of node input connections
			num_input_endpoints_list.push_back(1);					
			
			// set buffer schemes
			input_buffer_management_schemes.push_back(buffer_management_scheme);
			input_buffer_size_list.push_back(router_input_buffer_size);
      
      }
		// INGRESS OUTPUT
		LOG_PRINT("Creating INGRESS output...");
      // add connections to all middle routers at output
		for (UInt32 i = 0; i < _num_mid_routers; i++)
      {
			core_id_t coreID = computeMiddleCoreID((_cluster_id*_num_mid_routers)+i);  
			Router::Id router_id(coreID, MIDDLE_ROUTER);
			NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, router_id); 
			num_output_endpoints_list.push_back(1);	
			LOG_PRINT("Ingress output router id: %i, %i", coreID, MIDDLE_ROUTER);
			
			// set buffer management scheme according to config file
			downstream_buffer_management_schemes.push_back(buffer_management_scheme);
			downstream_buffer_size_list.push_back(router_input_buffer_size);
		}
   }
   
   else if (router_index == MIDDLE_ROUTER)
   {
	   // MIDDLE INPUT
	   LOG_PRINT("Creating MIDDLE input...");
      // for each input channel of middle router
	   // add connections to all ingress routers
		for (UInt32 i = 0; i < _num_in_routers; i++)
      {
			core_id_t coreID = computeIngressCoreID((mid_cluster_id*_num_in_routers) + i);      //use mid_cluster_id because want the cluster in which the middle router resides (not necessarily the same as the cluster of the CORE_INTERFACE with same ID)
			Router::Id router_id(coreID, INGRESS_ROUTER);
			NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, router_id); 
			num_input_endpoints_list.push_back(1);					
			LOG_PRINT("MIDDLE input router id: %i, %i", coreID, INGRESS_ROUTER);
			
			// set buffer schemes
			input_buffer_management_schemes.push_back(buffer_management_scheme);
			input_buffer_size_list.push_back(router_input_buffer_size);
		}
	   
	   // MIDDLE OUTPUT 
      LOG_PRINT("Creating MIDDLE output...");
	   // add connections to all egress routers
		for (UInt32 i = 0; i < _num_in_routers; i++)
      {
			LOG_PRINT("mid_cluster_id is %i", mid_cluster_id);
			core_id_t coreID = computeEgressCoreID((mid_cluster_id*_num_in_routers)+i); 
			Router::Id router_id(coreID, EGRESS_ROUTER);
			NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, router_id); 
			num_output_endpoints_list.push_back(1);	
			LOG_PRINT("Router id of MIDDLE output: (%i, %i)", coreID, EGRESS_ROUTER);

			// set buffer management scheme according to config file
			downstream_buffer_management_schemes.push_back(buffer_management_scheme);
			downstream_buffer_size_list.push_back(router_input_buffer_size);
		}
   }
  
	else //if (router_index == EGRESS_ROUTER)
	{
      // EGRESS INPUT
		LOG_PRINT("Creating EGRESS input...");
      // add connections to all ingress routers
		for (UInt32 i = 0; i < _num_mid_routers; i++)
      {
			core_id_t coreID = computeMiddleCoreID((_cluster_id*_num_mid_routers)+i);  
			Router::Id router_id(coreID, MIDDLE_ROUTER);
			NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, router_id); 
			num_input_endpoints_list.push_back(1);				       

			// set buffer schemes
			input_buffer_management_schemes.push_back(buffer_management_scheme);
			input_buffer_size_list.push_back(router_input_buffer_size);			
		}
	   
	    // EGRESS OUTPUT
      LOG_PRINT("Creating EGRESS output...");
		for (UInt32 i = 0; i < _num_router_ports; i++)
      {
			// add the core interfaces
			core_id_t coreID = computeEgressInterfaceCoreID(node_coreID, i);	
			Router::Id core_interface(coreID, CORE_INTERFACE);		
			NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, core_interface); //add this core to list of node input connections
			num_output_endpoints_list.push_back(1);					
         
			 // make downstream buffer scheme INFINITE since connects to CORE_INTERFACE
			 downstream_buffer_management_schemes.push_back(BufferManagementScheme::INFINITE);
			 downstream_buffer_size_list.push_back(-1);
 
		}
	}
	// now we have created input & output connections based on the type of network node
	// update number of input and output channels this node has
   SInt32 num_input_channels = input_channel_to_router_id_list__mapping.size();
	SInt32 num_output_channels = output_channel_to_router_id_list__mapping.size();
   LOG_PRINT("# input channels = %i, # output channels = %i", num_input_channels, num_output_channels);
  
   LOG_PRINT("Create router performance model...");
   // Create the router performance model
   RouterPerformanceModel* router_performance_model = \
         new RouterPerformanceModel( \
               _flow_control_scheme, \
               data_pipeline_delay, \
               credit_pipeline_delay, \
               num_input_channels, num_output_channels, \
               num_input_endpoints_list, num_output_endpoints_list, \
               input_buffer_management_schemes, downstream_buffer_management_schemes, \
               input_buffer_size_list, downstream_buffer_size_list);

   LOG_PRINT("Create router power model...");
   // Create the router power model
   RouterPowerModel* router_power_model = \
         RouterPowerModel::create(num_input_channels, num_output_channels, \
               router_input_buffer_size, _flit_width);

   LOG_PRINT("Create link models...");
   // Create the output link performance and power models
   vector<LinkPerformanceModel*> link_performance_model_list;
   vector<LinkPowerModel*> link_power_model_list; 
   for (SInt32 i = 0; i < num_output_channels; i++)
   {
      ElectricalLinkPerformanceModel* link_performance_model = \
         ElectricalLinkPerformanceModel::create(link_type, \
               _frequency, link_length, _flit_width, 1);
      link_performance_model_list.push_back(link_performance_model);

      ElectricalLinkPowerModel* link_power_model = \
         ElectricalLinkPowerModel::create(link_type, \
               _frequency, link_length, _flit_width, 1);
      link_power_model_list.push_back(link_power_model);
   }
  
   LOG_PRINT("Create the network node with Router Id [%i,%i]", node_coreID, router_index);
   // Now have all the ingredients, so
   // Create the router model 
   return new NetworkNode(Router::Id(node_coreID, router_index),
         _flit_width,
         router_performance_model,
         router_power_model,
         link_performance_model_list,
         link_power_model_list,
         input_channel_to_router_id_list__mapping,
         output_channel_to_router_id_list__mapping,
         _flow_control_packet_type);
}


// function used to determine coreIDs the ingress routers connect to
core_id_t 
FiniteBufferNetworkModelFlipAtac::computeInterfaceCoreID(core_id_t node_coreID, UInt32 i){
	// stage_index tells us which ingress router the head flit is at
	SInt32 stage_index = node_coreID/_num_router_ports;                    
	// now compute which coreID to connect to. 
	// NOTE*: assume that cores at input are labelled 0 to n, where n is the total number of cores
	SInt32 input_core_id = stage_index * _num_router_ports + i;
	return ((core_id_t) input_core_id); 							
}


// function used to determine coreIDs the egress routers connect to
core_id_t 
FiniteBufferNetworkModelFlipAtac::computeEgressInterfaceCoreID(core_id_t node_coreID, UInt32 i){
	// stage_index tells us which egress router the head flit is at
	SInt32 stage_index = (node_coreID + 1 - _num_router_ports)/_num_router_ports;               
	// now that compute which coreID to connect to. 
	SInt32 output_core_id = stage_index * _num_router_ports + i;
	return ((core_id_t) output_core_id); 								
}


// function used to determine the coreIDs of the middle routers in the clos
core_id_t
FiniteBufferNetworkModelFlipAtac::computeMiddleCoreID(UInt32 i){
	// use list created in constructor
	return _middle_coreID_list[i];
}

// function used to determine the coreIDs of ingress routers
core_id_t
FiniteBufferNetworkModelFlipAtac::computeIngressCoreID(UInt32 i){
	// use list created in constructor
	return _ingress_coreID_list[i];
}

// function used to determine the coreIDs of egress routers
core_id_t
FiniteBufferNetworkModelFlipAtac::computeEgressCoreID(UInt32 i){
	// use list created in constructor
	return _egress_coreID_list[i];
}

Router::Id
FiniteBufferNetworkModelFlipAtac::computeIngressRouterId(core_id_t core_id)
{
   assert(core_id == _core_id);
   // each CORE_INTERFACE connects directly to BCAST_ROUTER with same core_id
   return Router::Id(core_id, BCAST_ROUTER);
}

// function to return a random MIDDLE_ROUTER index
UInt32
FiniteBufferNetworkModelFlipAtac::getRandNum(UInt32 start, UInt32 end)
{
   double result;
   drand48_r(&_rand_data_buffer, &result);
   return (SInt32) (start + result * (end - start));
}

// print to sim.out file
void
FiniteBufferNetworkModelFlipAtac::outputSummary(ostream& out)
{
   NetworkModel::outputSummary(out);
   outputEventCountSummary(out);
}

// print event counts to sim.out file
void
FiniteBufferNetworkModelFlipAtac::outputEventCountSummary(ostream& out)
{
   out << "  Event Counters: " << endl;
   out << "    Bcast Total Input Buffer Writes: " << _network_node_map[BCAST_ROUTER]->getTotalInputBufferWrites() << endl;
   out << "    Bcast Total Input Buffer Reads: " << _network_node_map[BCAST_ROUTER]->getTotalInputBufferReads() << endl;
   out << "    Bcast Total Switch Allocator Requests: " << _network_node_map[BCAST_ROUTER]->getTotalSwitchAllocatorRequests() << endl;
   out << "    Bcast Total Crossbar Traversals: " << _network_node_map[BCAST_ROUTER]->getTotalCrossbarTraversals(1) << endl;
   UInt64 bcast_link_traversals = _network_node_map[BCAST_ROUTER]->getTotalOutputLinkUnicasts(Channel::ALL) +
                                  _network_node_map[BCAST_ROUTER]->getTotalOutputLinkBroadcasts(Channel::ALL);
   out << "    Bcast Total Link Traversals: " << bcast_link_traversals << endl;
   
   out << "    Mux Total Input Buffer Writes: " << _network_node_map[MUX_ROUTER]->getTotalInputBufferWrites() << endl;
   out << "    Mux Total Input Buffer Reads: " << _network_node_map[MUX_ROUTER]->getTotalInputBufferReads() << endl;
   out << "    Mux Total Switch Allocator Requests: " << _network_node_map[MUX_ROUTER]->getTotalSwitchAllocatorRequests() << endl;
   out << "    Mux Total Crossbar Traversals: " << _network_node_map[MUX_ROUTER]->getTotalCrossbarTraversals(1) << endl;
   UInt64 mux_link_traversals = _network_node_map[MUX_ROUTER]->getTotalOutputLinkUnicasts(Channel::ALL) +
                                _network_node_map[MUX_ROUTER]->getTotalOutputLinkBroadcasts(Channel::ALL);
   out << "    Mux Total Link Traversals: " << mux_link_traversals << endl;
   
   out << "    Ingress Total Input Buffer Writes: " << _network_node_map[INGRESS_ROUTER]->getTotalInputBufferWrites() << endl;
   out << "    Ingress Total Input Buffer Reads: " << _network_node_map[INGRESS_ROUTER]->getTotalInputBufferReads() << endl;
   out << "    Ingress Total Switch Allocator Requests: " << _network_node_map[INGRESS_ROUTER]->getTotalSwitchAllocatorRequests() << endl;
   out << "    Ingress Total Crossbar Traversals: " << _network_node_map[INGRESS_ROUTER]->getTotalCrossbarTraversals(1) << endl;
   UInt64 ingress_link_traversals = _network_node_map[INGRESS_ROUTER]->getTotalOutputLinkUnicasts(Channel::ALL) +
                                    _network_node_map[INGRESS_ROUTER]->getTotalOutputLinkBroadcasts(Channel::ALL);
   out << "    Ingress Total Link Traversals: " << ingress_link_traversals << endl;
   
   out << "    Middle  Total Input Buffer Writes: " << _network_node_map[MIDDLE_ROUTER]->getTotalInputBufferWrites() << endl;
   out << "    Middle  Total Input Buffer Reads: " << _network_node_map[MIDDLE_ROUTER]->getTotalInputBufferReads() << endl;
   out << "    Middle  Total Switch Allocator Requests: " << _network_node_map[MIDDLE_ROUTER]->getTotalSwitchAllocatorRequests() << endl;
   out << "    Middle  Total Crossbar Traversals: " << _network_node_map[MIDDLE_ROUTER]->getTotalCrossbarTraversals(1) << endl;
   UInt64 middle_link_traversals = _network_node_map[MIDDLE_ROUTER]->getTotalOutputLinkUnicasts(Channel::ALL) +
                                   _network_node_map[MIDDLE_ROUTER]->getTotalOutputLinkBroadcasts(Channel::ALL);
   out << "    Middle  Total Link Traversals: " << middle_link_traversals << endl;
   
   out << "    Egress  Total Input Buffer Writes: " << _network_node_map[EGRESS_ROUTER]->getTotalInputBufferWrites() << endl;
   out << "    Egress  Total Input Buffer Reads: " << _network_node_map[EGRESS_ROUTER]->getTotalInputBufferReads() << endl;
   out << "    Egress  Total Switch Allocator Requests: " << _network_node_map[EGRESS_ROUTER]->getTotalSwitchAllocatorRequests() << endl;
   out << "    Egress  Total Crossbar Traversals: " << _network_node_map[EGRESS_ROUTER]->getTotalCrossbarTraversals(1) << endl;
   UInt64 egress_link_traversals = _network_node_map[EGRESS_ROUTER]->getTotalOutputLinkUnicasts(Channel::ALL) +
                                   _network_node_map[EGRESS_ROUTER]->getTotalOutputLinkBroadcasts(Channel::ALL);
   out << "    Egress  Total Link Traversals: " << egress_link_traversals << endl;
}



pair<bool,UInt32>
FiniteBufferNetworkModelFlipAtac::computeCoreCountConstraints(SInt32 core_count)
{
   UInt32 N = (UInt32) core_count;
   UInt32 _num_router_ports, _num_in_routers, _num_mid_routers, _num_clusters; 
   readTopologyParams(_num_router_ports,_num_in_routers, _num_mid_routers, _num_clusters);
   
   if ((N != (_num_router_ports * _num_in_routers * _num_clusters)) )
   {
      fprintf(stderr, "[[ N(%i) != m(%i) * n(%i) ]] OR [[ N(%i) < r(%i) ]]\n",
            N, _num_router_ports, _num_in_routers, N, _num_mid_routers);
      exit(-1);
   }
   
   return make_pair<bool,UInt32>(false,N);
}


// put memory controllers on middle routers (and ingress if not enough middle)
pair<bool,vector<core_id_t> > 
FiniteBufferNetworkModelFlipAtac::computeMemoryControllerPositions(SInt32 num_memory_controllers)
{
      // read topology param
      UInt32 _num_router_ports, _num_in_routers, _num_mid_routers, _num_clusters;
      readTopologyParams(_num_router_ports, _num_in_routers, _num_mid_routers, _num_clusters);

      // make list of middle and ingress router core Ids for whole network (all clusters)
      // INGRESS_ROUTER list of coreIDs
      vector<core_id_t> ingress_coreID_list;
      UInt32 k;
      for (k = 0; k < (_num_in_routers * _num_clusters); k++)
      {
         ingress_coreID_list.push_back(k * _num_router_ports);		//this is the mapping convention for ingress routers
      }  
      
      // MIDDLE_ROUTER list of coreIDs
      vector<core_id_t> middle_coreID_list;
      SInt32 num_in_cores = _num_in_routers * _num_router_ports;
      for (k = 0; k < (_num_mid_routers * _num_clusters); k++)
      {
         middle_coreID_list.push_back( (k * (num_in_cores/_num_mid_routers)) + 1);
      }

      
      vector<core_id_t> core_id_list_with_memory_controllers;
      //put memory controllers on middle routers
      SInt32 i;
      for (i = 0; (i < num_memory_controllers && i < (SInt32) middle_coreID_list.size()); i++)
      {
         core_id_list_with_memory_controllers.push_back(middle_coreID_list[i]);
      }
      //if don't have enough middle routers, start putting remaining mem controllers on ingress routers
      for (SInt32 j = 0; (j < (num_memory_controllers - i) && j < (SInt32) ingress_coreID_list.size()); j++)
      {
         core_id_list_with_memory_controllers.push_back(ingress_coreID_list[j]); 
      }

   return (make_pair(true, core_id_list_with_memory_controllers));
}

