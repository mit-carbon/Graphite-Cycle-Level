#include <algorithm> //??
#include <cmath>	//??

#include "router.h"
#include "router_performance_model.h"
#include "router_power_model.h"
#include "electrical_link_performance_model.h"
#include "electrical_link_power_model.h"
#include "finite_buffer_network_model_clos.h"
#include "simulator.h"
#include "config.h"
#include "core.h"   
#include "log.h"

FiniteBufferNetworkModelClos::FiniteBufferNetworkModelClos(Network* network, SInt32 network_id):
   FiniteBufferNetworkModel(network, network_id)
{
   LOG_PRINT("Enter FiniteBufferNetworkModelClos with core_id %i", getNetwork()->getCore()->getId());
    
   // Initialize Clos Topology Parameters from user input in config file
   readTopologyParams(num_router_ports, num_in_routers, num_mid_routers);
   LOG_PRINT("num_router ports = %u, num_in_routers = %u, num_mid_routers = %u", num_router_ports, num_in_routers, num_mid_routers);
   
   // Router, Link Params
   SInt32 num_flits_per_output_port = 0;
   string link_type;
   UInt64 link_delay;
   UInt64 router_delay;
   
   volatile double link_length;

   // Contention Model Params
   bool contention_model_enabled = false;
   string contention_model_type;
   
   // Get Network Parameters
   try
   {
      _frequency = Sim()->getCfg()->getFloat("network/clos/frequency");

      num_flits_per_output_port = Sim()->getCfg()->getInt("network/clos/router/num_flits_per_port_buffer");
      router_delay = Sim()->getCfg()->getInt("network/clos/router/delay");
      link_delay = Sim()->getCfg()->getInt("network/clos/link/delay");
      link_type = Sim()->getCfg()->getString("network/clos/link/type");
      link_length = Sim()->getCfg()->getFloat("network/clos/link/length");
      _flit_width = Sim()->getCfg()->getInt("network/clos/link/width");    //variable inherited from finite_buffer_network_model

      // Contention Model parameters
      contention_model_enabled = Sim()->getCfg()->getBool("network/clos/queue_model/enabled");
      contention_model_type = Sim()->getCfg()->getString("network/clos/queue_model/type");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Unable to read Clos network parameters from cfg file");
   }

    // determine which cores have ingress, middle and/or egress routers mapped to them
	// store these coreIDs in list for ingress, middle and/or egress
	LOG_PRINT("Computing which cores have ingress, middle, and/or egress routers...");
	// INGRESS_ROUTER list of coreIDs
   for (UInt32 i = 0; i < num_in_routers; i++)
   {
      ingress_coreID_list.push_back(i * num_router_ports);		//this is the mapping convention for ingress routers
   }
	
	// MIDDLE_ROUTER list of coreIDs
   SInt32 num_in_cores = num_in_routers * num_router_ports;
   for (UInt32 i = 0; i < num_mid_routers; i++)
   {
      middle_coreID_list.push_back( (i * (num_in_cores/num_mid_routers)) + 1);
   }

	// EGRESS_ROUTER list of coreIDs
   for (UInt32 i = 0; i < num_in_routers; i++)
   {
      egress_coreID_list.push_back( (i * num_router_ports) + num_router_ports-1);
   }
   
   //Get this core's id
   core_id_t core_id = getNetwork()->getCore()->getId();		
   
   //Always create all types of nodes (INGRESS, MIDDLE, EGRESS) for each core. 
   //Note: this is to comply with the finite buffer model framework (indexing in _network_node_list vector)
   LOG_PRINT("Create INGRESS_ROUTER for core_id %i", core_id);
   //create INGRESS_ROUTER for this core
	NetworkNode* ingress_router = createNetworkNode(core_id, INGRESS_ROUTER);
	//add this node to the list of network nodes associated with this core (list is declared protected in "finite_buffer_network_model.h")
	_network_node_list.push_back(ingress_router);
         
   LOG_PRINT("Create MIDDLE_ROUTER for core_id %i", core_id);
   //create MIDDLE_ROUTER for this core
	NetworkNode* middle_router = createNetworkNode(core_id, MIDDLE_ROUTER);
	//add this node to the list of network nodes associated with this core (list is declared protected in "finite_buffer_network_model.h")
	_network_node_list.push_back(middle_router);
	
   LOG_PRINT("Create EGRESS_ROUTER for core_id %i", core_id);
   //create EGRESS_ROUTER for this core
	NetworkNode* egress_router = createNetworkNode(core_id, EGRESS_ROUTER);
	//add this node to the list of network nodes associated with this core (list is declared protected in "finite_buffer_network_model.h")
	_network_node_list.push_back(egress_router);
   
  
   num_nodes_on_core = _network_node_list.size();		//update number of nodes on this core
   LOG_PRINT("Core_id %i has %i network nodes", core_id, num_nodes_on_core);
   
   // Seed the buffer for random number generation
   srand48_r(core_id, &_rand_data_buffer);
   
   LOG_PRINT("Exit FiniteBufferNetworkModelClos constructor core_id %i", core_id);
}

FiniteBufferNetworkModelClos::~FiniteBufferNetworkModelClos()
{
  // LOG_PRINT("Clos destructor.");
   // Delete the router objects
   vector<NetworkNode*>::iterator it = _network_node_list.begin();
   for ( ; it != _network_node_list.end(); it ++)
      delete (*it);
}

void
FiniteBufferNetworkModelClos::readTopologyParams(UInt32& num_router_ports, UInt32& num_in_routers, UInt32& num_mid_routers)
{
   try
   {
      num_router_ports = Sim()->getCfg()->getInt("network/clos/num_router_ports");
      num_in_routers = Sim()->getCfg()->getInt("network/clos/num_in_routers");
      num_mid_routers = Sim()->getCfg()->getInt("network/clos/num_mid_routers");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read Clos Topology Params from cfg file");
   }
}


// Function makes ChannelEndpointList in head flit object which contains all the next destinations of the flit
// This is the main routing function.
void
FiniteBufferNetworkModelClos::computeOutputEndpointList(Flit* head_flit, NetworkNode* curr_network_node)
{
   LOG_PRINT("computeOutputEndpointList: head_flit, curr_network _node =(%p,%p) enter", head_flit, curr_network_node);
   LOG_PRINT("head_flit->_sender %i, head_flit->_receiver %i", head_flit->_sender, head_flit->_receiver);
   
   // get routerID from curr_network_node to determine if core, ingress, middle, or egress
	// for each case, will determine the next destinations and put them in head flit
   Router::Id curr_router_id = curr_network_node->getRouterId();
   core_id_t curr_core_id = curr_router_id._core_id;
   SInt32 curr_router_index = curr_router_id._index;
   LOG_PRINT("Current Router Id [%i,%i])", curr_router_id._core_id, curr_router_id._index);
   
   assert(_core_id == curr_router_id._core_id);
   
   list<Router::Id> next_dest_list;     //list of next destination routerIDs -->but will only add one next destination?!!!! --> no purpose to this, i think
   vector<Channel::Endpoint> output_endpoint_vec;

   if (curr_router_index == INGRESS_ROUTER){
      LOG_PRINT("At INGRESS_ROUTER with core_id %i. Compute next destination.", curr_core_id);
      // next destination is a random middle router
      UInt32 middle_router_idx = getRandNum(0, num_mid_routers);                 // get random index
      UInt32 N = num_in_routers * num_router_ports;
      core_id_t next_coreID = (middle_router_idx * (N/num_mid_routers)) + 1;     // compute coreID of this random middle router from index
      
      Router::Id router_id(next_coreID, MIDDLE_ROUTER); //getRouterIndexFromCoreId(next_coreID, MIDDLE_ROUTER));
      //add next router to the list
      next_dest_list.push_back(router_id);
      LOG_PRINT("Next destination Router Id [%i,%i]", router_id._core_id, router_id._index);
      
      //add corresponding channel endpoint to the vector (will later be added to the head flit object)
      Channel::Endpoint& output_endpoint = curr_network_node->getOutputEndpointFromRouterId(router_id);        //check that the routerID in param of function is supposed to be *next* (not curr) router!!!
      output_endpoint_vec.push_back(output_endpoint);
   }
	
   //if at MIDDLE_ROUTER
   else if (curr_router_index == MIDDLE_ROUTER) {//(curr_router_index == getRouterIndexFromCoreId(curr_core_id, MIDDLE_ROUTER)){
      LOG_PRINT("At MIDDLE_ROUTER with core_id %i. Compute next destination.", curr_core_id);
      // next destination is the correct output router
      
      //compute coreID of egress router the receiving core is connected to (based on coreID)
      //simular computation as for sending core connections to ingress routers
      core_id_t next_coreID = egress_coreID_list[head_flit->_receiver/num_router_ports];     //note: casting issue?
      
      //this is router id for next destination
      Router::Id router_id(next_coreID, EGRESS_ROUTER); //getRouterIndexFromCoreId(next_coreID, EGRESS_ROUTER));
      LOG_PRINT("Next destination Router Id [%i,%i]", router_id._core_id, router_id._index);
      
      //add it to the list
      next_dest_list.push_back(router_id);
      
      //add corresponding channel endpoint to the vector (will later be added to the head flit object)
      Channel::Endpoint& output_endpoint = curr_network_node->getOutputEndpointFromRouterId(router_id);        //check that the routerID in param of function is supposed to be *next* (not curr) router!!!
      output_endpoint_vec.push_back(output_endpoint);
   }
   
   //if at EGRESS_ROUTER
   else if (curr_router_index == EGRESS_ROUTER) {//(curr_router_index == getRouterIndexFromCoreId(curr_core_id, EGRESS_ROUTER)){
      LOG_PRINT("At EGRESS_ROUTER with core_id %i. Compute next destination.", curr_core_id);
      // next destination is the correct receiving core
      core_id_t next_coreID = head_flit->_receiver;
      
      //this is router id for next destination
      Router::Id  router_id(next_coreID, CORE_INTERFACE);
      LOG_PRINT("Next destination Router Id [%i,%i]", router_id._core_id, router_id._index);
       //add it to the list
      next_dest_list.push_back(router_id);
      
      //add corresponding channel endpoint to the vector (will later be added to the head flit object)
      Channel::Endpoint& output_endpoint = curr_network_node->getOutputEndpointFromRouterId(router_id);        //check that the routerID in param of function is supposed to be *next* (not curr) router!!!
      output_endpoint_vec.push_back(output_endpoint);
      
   }
     
   LOG_PRINT("Initialize head flit's channel endpoint list...");
   // In all cases...
   // Initialize the output channel struct inside head_flit
   head_flit->_output_endpoint_list = new ChannelEndpointList(output_endpoint_vec);
   
   LOG_PRINT("computeOutputEndpointList(%p, %p) end", head_flit, curr_network_node);

}


//creates network node of type router_index (INGRESS, MIDDLE, EGRESS, CORE) for the core with id node_coreID
//note: each core can only have one node (or none) of each type
NetworkNode*
FiniteBufferNetworkModelClos::createNetworkNode(core_id_t node_coreID, SInt32 router_index)
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
      buffer_management_scheme_str = Sim()->getCfg()->getString("network/clos/buffer_management_scheme"); 
      _flow_control_scheme = FlowControlScheme::parse(Sim()->getCfg()->getString("network/atac/flow_control_scheme")); //variable inherited from finite buff model
      data_pipeline_delay = Sim()->getCfg()->getInt("network/clos/router/data_pipeline_delay"); 
      credit_pipeline_delay = Sim()->getCfg()->getInt("network/clos/router/credit_pipeline_delay"); 
      router_input_buffer_size = Sim()->getCfg()->getInt("network/clos/router/input_buffer_size");
      link_type = Sim()->getCfg()->getString("network/clos/link/type"); 
      link_length = Sim()->getCfg()->getFloat("network/clos/link/length"); 
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
   vector<SInt32> num_input_endpoints_list;									//list of number of routers each input is connected to 
   vector<SInt32> num_output_endpoints_list;								//list of number of routers each output is connected to
   vector<BufferManagementScheme::Type> input_buffer_management_schemes;	//buffer management schemes for each input channel 
   vector<BufferManagementScheme::Type> downstream_buffer_management_schemes;//buffer management schemes for endpoint of each output channel
   vector<SInt32> input_buffer_size_list;									//buffer sizes at each input channel
   vector<SInt32> downstream_buffer_size_list;								//buffer sizes at endpoint of each output channel

   LOG_PRINT("Creating input and output channels of the network node...");
	//if ingress router
		//add connections to cores at input
		//add connections to other routers at output
		
	//if middle router
		//add connections to input routers
		//add connections to output routers
		
	//if egress router
		//add connections to input routers
		//add connections to cores at output
		
    if (router_index == INGRESS_ROUTER){
	   // INGRESS INPUT
	   LOG_PRINT("Creating INGRESS input...");
      // for each input channel of ingress router
		 for (UInt32 i = 0; i < num_router_ports; i++){
			 // add the core interfaces
			core_id_t coreID = computeInterfaceCoreID(node_coreID, i);	// compute the ID of each core at input of this router, given the coreID of this INGRESS router ?????????
			Router::Id core_interface(coreID, CORE_INTERFACE);		//create 'router' Id with coreId "coreID" and index "SENDING_CORE"
			NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, core_interface); //add this core to list of node input connections
			num_input_endpoints_list.push_back(1);					//each channel has one input
			
			 // make input buffer scheme infinite since connected to CORE_INTERFACE 
			 // need infinite buffer for core_interface to ensure network message type is DATA
			 input_buffer_management_schemes.push_back(BufferManagementScheme::INFINITE);
			 input_buffer_size_list.push_back(-1);
      
      }
		// INGRESS OUTPUT
		LOG_PRINT("Creating INGRESS output...");
      // add connections to all middle routers at output
		for (UInt32 i = 0; i < num_mid_routers; i++){
			core_id_t coreID = computeMiddleCoreID(node_coreID, i);  // i * (N/num_mid_routers)) + 1 ,   where N = num_router_ports * num_in_routers
			Router::Id router_id(coreID, MIDDLE_ROUTER);
			NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, router_id); 
			num_output_endpoints_list.push_back(1);	
			LOG_PRINT("Ingress output router id: %i, %i", coreID, MIDDLE_ROUTER);
			
			// set buffer management scheme according to config file
			downstream_buffer_management_schemes.push_back(buffer_management_scheme);
			downstream_buffer_size_list.push_back(router_input_buffer_size);
		}
    }
   
   else if (router_index == MIDDLE_ROUTER){
	   // MIDDLE INPUT
	   LOG_PRINT("Creating MIDDLE input...");
      // for each input channel of middle router
	   // add connections to all ingress routers
		for (UInt32 i = 0; i < num_in_routers; i++){
			core_id_t coreID = computeIngressCoreID(node_coreID, i);  // i *num_router_ports
			Router::Id router_id(coreID, INGRESS_ROUTER);
			NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, router_id); 
			num_input_endpoints_list.push_back(1);					
			
			// set buffer schemes
			input_buffer_management_schemes.push_back(buffer_management_scheme);
			input_buffer_size_list.push_back(router_input_buffer_size);
		}
	   
	   // MIDDLE OUTPUT 
      LOG_PRINT("Creating MIDDLE output...");
	   // add connections to all egress routers
		for (UInt32 i = 0; i < num_in_routers; i++){
			core_id_t coreID = computeEgressCoreID(node_coreID, i);  // (i * num_router_ports) + num_router_ports-1
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
		for (UInt32 i = 0; i < num_mid_routers; i++){
			core_id_t coreID = computeMiddleCoreID(node_coreID, i);  // j * (N/num_mid_routers)) + 1 ,   where N = num_router_ports * num_in_routers
			Router::Id router_id(coreID, MIDDLE_ROUTER);
			NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, router_id); 
			num_input_endpoints_list.push_back(1);				       

			// set buffer schemes
			input_buffer_management_schemes.push_back(buffer_management_scheme);
			input_buffer_size_list.push_back(router_input_buffer_size);			
		}
	   
	    // EGRESS OUTPUT
      LOG_PRINT("Creating EGRESS output...");
		for (UInt32 i = 0; i < num_router_ports; i++){
			// add the core interfaces
			core_id_t coreID = computeEgressInterfaceCoreID(node_coreID, i);	//????? compute coreID based on # of input routers and ports  --->>???????????chekc if correct i
			Router::Id core_interface(coreID, CORE_INTERFACE);		//create router Id with coreId "coreID" and index "RECEIVING_CORE"
			NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, core_interface); //add this core to list of node input connections
			num_output_endpoints_list.push_back(1);					//each channel connects to one core
         
			 //make downstream buffer scheme INFINITE since connects to CORE_INTERFACE
			 downstream_buffer_management_schemes.push_back(BufferManagementScheme::INFINITE);
			 downstream_buffer_size_list.push_back(-1);
 
		}
	}
	//now we have created input & output connections based on whether ingress, middle or egress node
	
	   
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


//function used to determine coreIDs the ingress routers connect to
core_id_t 
FiniteBufferNetworkModelClos::computeInterfaceCoreID(core_id_t node_coreID, UInt32 i){
	//stage_index tells me which ingress (or egress) router i am at
	SInt32 stage_index = node_coreID/num_router_ports;
	//now that i know which router i'm at, i know which coreID to connect to. 
	//NOTE*: assume that cores at input/output are labelled 0 to n, where n is the total number of cores
	SInt32 input_core_id = stage_index * num_router_ports + i;
	return ((core_id_t) input_core_id); 								//note: not sure if casting can work like this
}


//function used to determine coreIDs the egress routers connect to
core_id_t 
FiniteBufferNetworkModelClos::computeEgressInterfaceCoreID(core_id_t node_coreID, UInt32 i){
	//stage_index tells me which ingress (or egress) router i am at
	SInt32 stage_index = (node_coreID + 1 - num_router_ports)/num_router_ports;               
	//now that i know which router i'm at, i know which coreID to connect to. 
	//NOTE*: assume that cores at input/output are labelled 0 to n, where n is the total number of cores
	SInt32 output_core_id = stage_index * num_router_ports + i;
	return ((core_id_t) output_core_id); 								//note: not sure if casting can work like this
}


//function used to determine the coreIDs of the middle routers in the clos
core_id_t
FiniteBufferNetworkModelClos::computeMiddleCoreID(core_id_t node_coreID, UInt32 i){
	//use list created in constructor
	return middle_coreID_list[i];
}

core_id_t
FiniteBufferNetworkModelClos::computeIngressCoreID(core_id_t node_coreID, UInt32 i){
	//use list created in constructor
	return ingress_coreID_list[i];
}

core_id_t
FiniteBufferNetworkModelClos::computeEgressCoreID(core_id_t node_coreID, UInt32 i){
	//use list created in constructor
	return egress_coreID_list[i];
}

Router::Id
FiniteBufferNetworkModelClos::computeIngressRouterId(core_id_t core_id)
{
   //assert(core_id == _core_id);
   core_id_t ingress_core_id = core_id / num_router_ports * num_router_ports;
   return Router::Id(ingress_core_id, INGRESS_ROUTER);
}

//function to return random index of middle router
UInt32
FiniteBufferNetworkModelClos::getRandNum(UInt32 start, UInt32 end)
{
   double result;
   drand48_r(&_rand_data_buffer, &result);
   return (SInt32) (start + result * (end - start));
}


void
FiniteBufferNetworkModelClos::outputSummary(ostream& out)
{
   FiniteBufferNetworkModel::outputSummary(out);
}


//??????????????????????? copied from old eclos
pair<bool,UInt32>
FiniteBufferNetworkModelClos::computeCoreCountConstraints(SInt32 core_count)
{
   //LOG_PRINT("computeCoreCountConstraints for Clos with core_cout %i", core_count);
   // Check for m,n,r correctly
   UInt32 N = (UInt32) core_count;
   UInt32 num_router_ports, num_in_routers, num_mid_routers; 
   readTopologyParams(num_router_ports,num_in_routers, num_mid_routers);
   if ((N != (num_router_ports * num_in_routers)) )
   {
      fprintf(stderr, "[[ N(%i) != m(%i) * n(%i) ]] OR [[ N(%i) < r(%i) ]]\n",
            N, num_router_ports, num_in_routers, N, num_mid_routers);
      exit(-1);
   }
   return make_pair<bool,UInt32>(false,N);
}


//put memory controllers on middle routers (and ingress if not enough middle)
pair<bool,vector<core_id_t> > 
FiniteBufferNetworkModelClos::computeMemoryControllerPositions(SInt32 num_memory_controllers)
{
      //LOG_PRINT("computeMemoryControllerPositions for Clos with %i mem controllers", num_memory_controllers);
      //read topology param
      UInt32 num_router_ports, num_in_routers, num_mid_routers;
      readTopologyParams(num_router_ports, num_in_routers, num_mid_routers);

      //make list of middle and ingress router core Ids (note: cannot use middle_coreID_list because this is a static function)
      // INGRESS_ROUTER list of coreIDs
      vector<core_id_t> _ingress_coreID_list;
      UInt32 k;
      for (k = 0; k < num_in_routers; k++)
      {
         _ingress_coreID_list.push_back(k * num_router_ports);		//this is the mapping convention for ingress routers
      }  
      
      // MIDDLE_ROUTER list of coreIDs
      vector<core_id_t> _middle_coreID_list;
      SInt32 num_in_cores = num_in_routers * num_router_ports;
      for (k = 0; k < num_mid_routers; k++)
      {
         _middle_coreID_list.push_back( (k * (num_in_cores/num_mid_routers)) + 1);
      }

      
      vector<core_id_t> core_id_list_with_memory_controllers;
      //put memory controllers on middle routers
      SInt32 i;
      for (i = 0; (i < num_memory_controllers && i < (SInt32) _middle_coreID_list.size()); i++)
      {
         core_id_list_with_memory_controllers.push_back(_middle_coreID_list[i]);
      }
      //if don't have enough middle routers, start putting remaining mem controllers on ingress routers
      for (SInt32 j = 0; (j < (num_memory_controllers - i) && j < (SInt32) _ingress_coreID_list.size()); j++)
      {
         core_id_list_with_memory_controllers.push_back(_ingress_coreID_list[j]);    //note: since a core can have an ingress & mid router, could have same coreID-->fix this later
      }

   return (make_pair(true, core_id_list_with_memory_controllers));
}

