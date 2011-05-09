#include <cmath>

#include "router.h"
#include "router_performance_model.h"
#include "router_power_model.h"
#include "electrical_link_performance_model.h"
#include "electrical_link_power_model.h"
#include "finite_buffer_network_model_emesh.h"
#include "simulator.h"
#include "config.h"
#include "log.h"

FiniteBufferNetworkModelEMesh::FiniteBufferNetworkModelEMesh(Network* net, \
      SInt32 network_id, bool broadcast_enabled):
   FiniteBufferNetworkModel(net, network_id)
{
   if (broadcast_enabled)
      _emesh_network = "network/emesh_hop_by_hop_broadcast_tree/";
   else
      _emesh_network = "network/emesh_hop_by_hop_basic/";

   // Initialize EMesh Topology Parameters
   computeEMeshTopologyParameters(_emesh_width, _emesh_height);

   // Get Network Parameters
   try
   {
      _frequency = Sim()->getCfg()->getFloat(_emesh_network + "frequency");
      _flit_width = Sim()->getCfg()->getInt(_emesh_network + "flit_width");
      _flow_control_scheme = FlowControlScheme::parse( \
            Sim()->getCfg()->getString(_emesh_network + "flow_control_scheme"));
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read Electrical Mesh parameters from the cfg file");
   }

   // Instantiate the routers 
   NetworkNode* network_node = createNetworkNode();
   _network_node_list.push_back(network_node);
}

FiniteBufferNetworkModelEMesh::~FiniteBufferNetworkModelEMesh()
{}

NetworkNode*
FiniteBufferNetworkModelEMesh::createNetworkNode()
{
   // FIXME: Change channel -> link , channel -> port later
   // Read necessary parameters from cfg file
   string buffer_management_scheme_str;
   SInt32 data_pipeline_delay = 0;
   SInt32 credit_pipeline_delay = 0;
   SInt32 router_input_buffer_size = 0;
   string link_type;
   volatile double link_length = 0.0;

   try
   {
      buffer_management_scheme_str = Sim()->getCfg()->getString(_emesh_network + "buffer_management_scheme");
      data_pipeline_delay = Sim()->getCfg()->getInt(_emesh_network + "router/data_pipeline_delay");
      credit_pipeline_delay = Sim()->getCfg()->getInt(_emesh_network + "router/credit_pipeline_delay");
      router_input_buffer_size = Sim()->getCfg()->getInt(_emesh_network + "router/input_buffer_size");
      link_type = Sim()->getCfg()->getString(_emesh_network + "link/type");
      link_length = Sim()->getCfg()->getFloat(_emesh_network + "link/length");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read Electrical mesh parameters from the cfg file");
   }
     
   BufferManagementScheme::Type buffer_management_scheme = \
         BufferManagementScheme::parse(buffer_management_scheme_str);
   
   vector<vector<Router::Id> > input_channel_to_router_id_list__mapping;
   vector<vector<Router::Id> > output_channel_to_router_id_list__mapping;
   vector<SInt32> num_input_endpoints_list;
   vector<SInt32> num_output_endpoints_list;
   vector<BufferManagementScheme::Type> input_buffer_management_schemes;
   vector<BufferManagementScheme::Type> downstream_buffer_management_schemes;
   vector<SInt32> input_buffer_size_list;
   vector<SInt32> downstream_buffer_size_list;

   // Add the core interface
   Router::Id core_interface(_core_id, CORE_INTERFACE);
   NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, core_interface);
   NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, core_interface);
   num_input_endpoints_list.push_back(1);
   num_output_endpoints_list.push_back(1);

   SInt32 cx, cy;
   computeEMeshPosition(_core_id, cx, cy);

   // Add the channels to the adjoining routers
   SInt32 dx[4] = {-1,1,0,0};
   SInt32 dy[4] = {0,0,-1,1};
   for (SInt32 i = 0; i < 4; i++)
   {
      core_id_t core_id = computeCoreId(cx+dx[i], cy+dy[i]);
      if (core_id != INVALID_CORE_ID)
      {
         Router::Id router_id(core_id, EMESH);
         NetworkNode::addChannelMapping(input_channel_to_router_id_list__mapping, router_id);
         NetworkNode::addChannelMapping(output_channel_to_router_id_list__mapping, router_id);
         num_input_endpoints_list.push_back(1);
         num_output_endpoints_list.push_back(1);
      }
   }

   SInt32 num_input_channels = input_channel_to_router_id_list__mapping.size();
   SInt32 num_output_channels = output_channel_to_router_id_list__mapping.size();

   input_buffer_management_schemes.push_back(BufferManagementScheme::INFINITE);
   input_buffer_size_list.push_back(-1);
   for (SInt32 i = 1; i < num_input_channels; i++)
   {
      input_buffer_management_schemes.push_back(buffer_management_scheme);
      input_buffer_size_list.push_back(router_input_buffer_size);
   }

   downstream_buffer_management_schemes.push_back(BufferManagementScheme::INFINITE);
   downstream_buffer_size_list.push_back(-1);
   for (SInt32 i = 1; i < num_output_channels; i++)
   {
      downstream_buffer_management_schemes.push_back(buffer_management_scheme);
      downstream_buffer_size_list.push_back(router_input_buffer_size);
   }

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

   // Create the router power model
   RouterPowerModel* router_power_model = \
         RouterPowerModel::create(num_input_channels, num_output_channels, \
               router_input_buffer_size, _flit_width);

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
  
   // Create the router model 
   return new NetworkNode(Router::Id(_core_id, EMESH),
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
FiniteBufferNetworkModelEMesh::computeOutputEndpointList(Flit* head_flit, NetworkNode* curr_network_node)
{
   LOG_PRINT("computeOutputEndpointList(%p,%p) enter", head_flit, curr_network_node);

   Router::Id curr_router_id = curr_network_node->getRouterId();
   core_id_t curr_core_id = curr_router_id._core_id;
   SInt32 cx, cy;
   computeEMeshPosition(curr_core_id, cx, cy);

   list<core_id_t> next_dest_list;
   vector<Channel::Endpoint> output_endpoint_vec;

   if (head_flit->_receiver == NetPacket::BROADCAST)
   {
      SInt32 sx, sy;
      computeEMeshPosition(head_flit->_sender, sx, sy);

      if (cy >= sy)
         next_dest_list.push_back(computeCoreId(cx,cy+1));
      if (cy <= sy)
         next_dest_list.push_back(computeCoreId(cx,cy-1));
      if (cy == sy)
      {
         if (cx >= sx)
            next_dest_list.push_back(computeCoreId(cx+1,cy));
         if (cx <= sx)
            next_dest_list.push_back(computeCoreId(cx-1,cy));
         if (cx == sx)
            next_dest_list.push_back(_core_id);
      }

      // Eliminate cores that are not reachable
      list<core_id_t>::iterator it = next_dest_list.begin();
      while (it != next_dest_list.end())
      {
         core_id_t next_core_id = (*it);
         if (next_core_id == INVALID_CORE_ID)
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
      computeEMeshPosition(head_flit->_receiver, dx, dy);

      if (cx > dx)
      {
         next_dest_list.push_back(computeCoreId(cx-1,cy));
      }
      else if (cx < dx)
      {
         next_dest_list.push_back(computeCoreId(cx+1,cy));
      }
      else if (cy > dy)
      {
         next_dest_list.push_back(computeCoreId(cx,cy-1));
      }
      else if (cy < dy)
      {
         next_dest_list.push_back(computeCoreId(cx,cy+1));
      }
      else
      {
         next_dest_list.push_back(_core_id);
      }
   }

   list<core_id_t>::iterator it = next_dest_list.begin();
   for ( ; it != next_dest_list.end(); it ++)
   {
      SInt32 router_index;
      if (*it == _core_id)
         router_index = CORE_INTERFACE;
      else
         router_index = EMESH;

      Router::Id router_id(*it, router_index);
      Channel::Endpoint& output_endpoint = \
            curr_network_node->getOutputEndpointFromRouterId(router_id);
      output_endpoint_vec.push_back(output_endpoint);
      
      LOG_PRINT("Next Router(%i,%i), Output Endpoint(%i,%i)", \
            router_id._core_id, router_id._index, \
            output_endpoint._channel_id, output_endpoint._index);
   }

   // Initialize the output channel struct inside head_flit
   head_flit->_output_endpoint_list = new ChannelEndpointList(output_endpoint_vec);
   
   LOG_PRINT("computeOutputEndpointList(%p,%p) exit, channel_endpoint_list.size(%u)", \
         head_flit, curr_network_node, head_flit->_output_endpoint_list->size());
}

void
FiniteBufferNetworkModelEMesh::computeEMeshPosition(core_id_t core_id, SInt32& x, SInt32& y)
{
   x = core_id % _emesh_width;
   y = core_id / _emesh_width;
}

core_id_t
FiniteBufferNetworkModelEMesh::computeCoreId(SInt32 x, SInt32 y)
{
   if ((x < 0) || (x >= _emesh_width) || (y < 0) || (y >= _emesh_height))
      return INVALID_CORE_ID;
   else
      return (y * _emesh_width + x);
}

SInt32
FiniteBufferNetworkModelEMesh::computeEMeshDistance(core_id_t sender, core_id_t receiver)
{
   SInt32 sx, sy, dx, dy;
   computeEMeshPosition(sender, sx, sy);
   computeEMeshPosition(receiver, dx, dy);

   return (abs(sx-dx) + abs(sy-dy));
}

void
FiniteBufferNetworkModelEMesh::computeEMeshTopologyParameters(SInt32& emesh_width, SInt32& emesh_height)
{
   SInt32 core_count = Config::getSingleton()->getTotalCores();

   emesh_width = (SInt32) floor (sqrt(core_count));
   emesh_height = (SInt32) ceil (1.0 * core_count / emesh_width);
   LOG_ASSERT_ERROR(core_count == (emesh_width * emesh_height),
         "total_cores(%i), emesh_width(%i), emesh_height(%i)",
         core_count, emesh_width, emesh_height);
}

pair<bool,SInt32>
FiniteBufferNetworkModelEMesh::computeCoreCountConstraints(SInt32 core_count)
{
   // This is before 'total_cores' is decided
   SInt32 emesh_width = (SInt32) floor (sqrt(core_count));
   SInt32 emesh_height = (SInt32) ceil (1.0 * core_count / emesh_width);

   assert(core_count <= emesh_width * emesh_height);
   assert(core_count > (emesh_width - 1) * emesh_height);
   assert(core_count > emesh_width * (emesh_height - 1));

   return make_pair(true, emesh_height * emesh_width);
}

pair<bool, vector<core_id_t> >
FiniteBufferNetworkModelEMesh::computeMemoryControllerPositions(SInt32 num_memory_controllers)
{
   // core_id_list_along_perimeter : list of cores along the perimeter of 
   // the chip in clockwise order starting from (0,0)
   
   // Initialize mesh_width, mesh_height
   SInt32 emesh_width, emesh_height;
   computeEMeshTopologyParameters(emesh_width, emesh_height);

   vector<core_id_t> core_id_list_along_perimeter;

   for (SInt32 i = 0; i < emesh_width; i++)
      core_id_list_along_perimeter.push_back(i);
   
   for (SInt32 i = 1; i < (emesh_height-1); i++)
      core_id_list_along_perimeter.push_back((i * emesh_width) + emesh_width-1);

   for (SInt32 i = emesh_width-1; i >= 0; i--)
      core_id_list_along_perimeter.push_back(((emesh_height-1) * emesh_width) + i);

   for (SInt32 i = emesh_height-2; i >= 1; i--)
      core_id_list_along_perimeter.push_back(i * emesh_width);

   assert(core_id_list_along_perimeter.size() == (UInt32) (2 * (emesh_width + emesh_height - 2)));

   LOG_ASSERT_ERROR(core_id_list_along_perimeter.size() >= (UInt32) num_memory_controllers,
         "num cores along perimeter(%u), num memory controllers(%i)",
         core_id_list_along_perimeter.size(), num_memory_controllers);

   SInt32 spacing_between_memory_controllers = core_id_list_along_perimeter.size() / num_memory_controllers;
   
   // core_id_list_with_memory_controllers : list of cores that have memory controllers attached to them
   vector<core_id_t> core_id_list_with_memory_controllers;

   for (SInt32 i = 0; i < num_memory_controllers; i++)
   {
      SInt32 index = (i * spacing_between_memory_controllers + emesh_width/2) % core_id_list_along_perimeter.size();
      core_id_list_with_memory_controllers.push_back(core_id_list_along_perimeter[index]);
   }

   return (make_pair(true, core_id_list_with_memory_controllers));
}

void
FiniteBufferNetworkModelEMesh::outputSummary(ostream& out)
{
   FiniteBufferNetworkModel::outputSummary(out);
}
