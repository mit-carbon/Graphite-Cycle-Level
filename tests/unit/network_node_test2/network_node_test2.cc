#include "network_node_test2.h"
#include "simulator.h"
#include "core_manager.h"
#include "core.h"
#include "network.h"
#include "finite_buffer_network_model.h"
#include "network_node.h"
#include "router_performance_model.h"
#include "flow_control_scheme.h"
#include "buffer_management_scheme.h"
#include "router_power_model.h"
#include "electrical_link_performance_model.h"
#include "link_power_model.h"
#include "network_msg.h"
#include "flit.h"
#include "head_flit.h"
#include "credit_msg.h"
#include "on_off_msg.h"
#include "carbon_user.h"

int main(int argc, char *argv[])
{
   // Start simulation
   CarbonStartSim(argc, argv);

   SInt32 num_channels = 5;
   SInt32 fanout = 4;

   // Router Performance Model
   FlowControlScheme::Type flow_control_scheme = FlowControlScheme::WORMHOLE;
   SInt32 data_pipeline_delay = 1;
   SInt32 credit_pipeline_delay = 1;

   // Get Finite Buffer Network Model
   Core* core = Sim()->getCoreManager()->getCoreFromID(0);
   assert(core);
   NetworkModel* network_model = core->getNetwork()->getNetworkModelFromPacketType(USER_2);
   assert(network_model->isFiniteBuffer());
   FiniteBufferNetworkModel* finite_buffer_network_model = (FiniteBufferNetworkModel*) network_model;

   vector<SInt32> num_input_endpoints_list(num_channels, 1);
   vector<SInt32> num_output_endpoints_list(num_channels, fanout);
   vector<BufferManagementScheme::Type> input_buffer_management_schemes(num_channels, BufferManagementScheme::CREDIT);
   vector<BufferManagementScheme::Type> downstream_buffer_management_schemes(num_channels, BufferManagementScheme::CREDIT);
   vector<SInt32> input_buffer_size_list(num_channels, 4);
   vector<SInt32> downstream_buffer_size_list(num_channels, 4);
   
   RouterPerformanceModel* router_performance_model = new RouterPerformanceModel(
         flow_control_scheme,
         data_pipeline_delay, credit_pipeline_delay,
         num_channels, num_channels,
         num_input_endpoints_list, num_output_endpoints_list,
         input_buffer_management_schemes, downstream_buffer_management_schemes,
         input_buffer_size_list, downstream_buffer_size_list);

   // Link Performance Model
   UInt32 flit_width = 64;
   double link_length = 1.0;
   float frequency = 1; // In GHz
   string link_type = "electrical_repeated";

   LinkPerformanceModel* link_performance_model = 
      ElectricalLinkPerformanceModel::create(link_type, frequency, link_length, flit_width, 1);
   vector<LinkPerformanceModel*> link_performance_model_list(num_channels, link_performance_model);

   // Network Node
   vector<vector<Router::Id> > input_channel_to_router_id_list__mapping;
   vector<vector<Router::Id> > output_channel_to_router_id_list__mapping;
   for (SInt32 i = 0; i < num_channels; i++)
   {
      Router::Id router_id(0, i);
      vector<Router::Id> router_id_list(1, router_id);
      input_channel_to_router_id_list__mapping.push_back(router_id_list);
   }
   for (SInt32 i = 0; i < num_channels; i++)
   {
      vector<Router::Id> router_id_list;
      for (SInt32 j = 0; j < fanout; j++)
      {
         Router::Id router_id(0, j*num_channels + i);
         router_id_list.push_back(router_id);
      }
      output_channel_to_router_id_list__mapping.push_back(router_id_list);
   }

   NetworkNode* network_node = new NetworkNode(
         Router::Id(0, num_channels*fanout),
         flit_width,
         router_performance_model,
         NULL,
         link_performance_model_list,
         vector<LinkPowerModel*>(num_channels, NULL),
         input_channel_to_router_id_list__mapping,
         output_channel_to_router_id_list__mapping,
         USER_2);

   finite_buffer_network_model->setNetworkNode(num_channels, network_node);
   
   for (SInt32 i = 0; i < num_channels * fanout; i++)
   {
      RouterPerformanceModel* router_performance_model = new RouterPerformanceModel(
            flow_control_scheme,
            data_pipeline_delay, credit_pipeline_delay,
            1,1,
            vector<SInt32>(1,1),
            vector<SInt32>(1,1),
            vector<BufferManagementScheme::Type>(1,BufferManagementScheme::CREDIT),
            vector<BufferManagementScheme::Type>(1,BufferManagementScheme::CREDIT),
            vector<SInt32>(1,4),
            vector<SInt32>(1,4));
      LinkPerformanceModel* link_performance_model =
         ElectricalLinkPerformanceModel::create(link_type, frequency, link_length, flit_width, 1);
      
      NetworkNode* network_node =
         new NetworkNode(
               Router::Id(0,i),
               flit_width,
               router_performance_model,
               NULL,
               vector<LinkPerformanceModel*>(1,link_performance_model),
               vector<LinkPowerModel*>(1,NULL),
               vector<vector<Router::Id> >(1, vector<Router::Id>(1, Router::Id(0, num_channels*fanout)) ),
               vector<vector<Router::Id> >(1, vector<Router::Id>(1, Router::Id(0, num_channels*fanout)) ),
               USER_2);

      finite_buffer_network_model->setNetworkNode(i, network_node);
   }

   list<NetPacket*> input_net_packet_list = createNetPacketList(); 
   printNetPacketList(input_net_packet_list, true);

   for (list<NetPacket*>::iterator it = input_net_packet_list.begin();
         it != input_net_packet_list.end(); it ++)
   {
      fprintf(stderr, "\n==============================================================================\n\n");
      printNetPacket(*it, true);

      list<NetPacket*> output_net_packet_list;
      network_node->processNetPacket(*it, output_net_packet_list);
      
      printNetPacketList(output_net_packet_list);
      
      destroyNetPacketList(output_net_packet_list);
   }

   // Stop simulation
   CarbonStopSim();

   return 0;
}

void printNetPacketList(list<NetPacket*>& net_packet_list, bool is_input_msg)
{
   for (list<NetPacket*>::iterator it = net_packet_list.begin(); it != net_packet_list.end(); it ++)
      printNetPacket(*it, is_input_msg);
}

void printNetPacket(NetPacket* net_packet, bool is_input_msg)
{
   fprintf(stderr, "\n\n");
   
   NetworkMsg* network_msg = (NetworkMsg*) net_packet->data;

   fprintf(stderr, "Network Msg [Type(%s), Time(%llu), Normalized Time(%llu), Sender Router(%i,%i), Receiver Router(%i,%i), Input Endpoint(%i,%i), Output Endpoint(%i,%i)]\n",
         network_msg->getTypeString().c_str(),
         (long long unsigned int) net_packet->time,
         (long long unsigned int) network_msg->_normalized_time,
         net_packet->sender, network_msg->_sender_router_index,
         net_packet->receiver, network_msg->_receiver_router_index,
         network_msg->_input_endpoint._channel_id, network_msg->_input_endpoint._index,
         network_msg->_output_endpoint._channel_id, network_msg->_output_endpoint._index);

   switch (network_msg->_type)
   {

   case NetworkMsg::DATA:
      
      {
         Flit* flit = (Flit*) network_msg;
         fprintf(stderr, "Flit [Type(%s), Normalized Time at Entry(%llu), Delay(%llu), Length(%i), Sender(%i), Receiver(%i), Net Packet(%p)]\n",
               flit->getTypeString().c_str(),
               (long long unsigned int) flit->_normalized_time_at_entry,
               (long long unsigned int) (flit->_normalized_time - flit->_normalized_time_at_entry),
               flit->_num_phits,
               flit->_sender,
               flit->_receiver,
               flit->_net_packet);

         if ((flit->_type & Flit::HEAD) && (is_input_msg))
         {
            HeadFlit* head_flit = (HeadFlit*) flit;
            vector<Channel::Endpoint>* endpoint_list = head_flit->_output_endpoint_list;
            fprintf(stderr, "Head Flit [Output Endpoint List( ");
            
            vector<Channel::Endpoint>::iterator it = endpoint_list->begin();
            for ( ; it != endpoint_list->end(); it ++)
            {
               Channel::Endpoint endpoint = *it;
               fprintf(stderr, "(%i,%i), ", endpoint._channel_id, endpoint._index);
            }
            fprintf(stderr, ")]\n");
         }
      }

      break;

   case NetworkMsg::BUFFER_MANAGEMENT:
            
      {
         BufferManagementMsg* buffer_msg = (BufferManagementMsg*) network_msg;
         fprintf(stderr, "Buffer Management Msg [Type(%s)]\n", buffer_msg->getTypeString().c_str());

         if (buffer_msg->_type == BufferManagementScheme::CREDIT)
         {
            CreditMsg* credit_msg = (CreditMsg*) buffer_msg;
            fprintf(stderr, "Credit Msg [Num Credits(%i)]\n", credit_msg->_num_credits);            
         }
         else if (buffer_msg->_type == BufferManagementScheme::ON_OFF)
         {
            OnOffMsg* on_off_msg = (OnOffMsg*) buffer_msg;
            fprintf(stderr, "On Off Msg [Status(%s)]\n", on_off_msg->_on_off_status ? "TRUE" : "FALSE");
         }
      }

      break;

   default:
      fprintf(stderr, "Unrecognized Network Msg Type(%u)", network_msg->_type);
      break;
   }
}

list<NetPacket*> createNetPacketList()
{
   list<NetPacket*> net_packet_list;
   UnstructuredBuffer output_endpoints;

   // Head Flit - 0 -> [ (0,1), (3,ALL), (4,0) ]
   output_endpoints << 0 << 1 << 3 << Channel::Endpoint::ALL << 4 << 0;
   addFlit(net_packet_list, Flit::HEAD, 5, 1, Router::Id(0,1), Router::Id(0,20), 3, &output_endpoints);

   // Head Flit - 1
   output_endpoints << 0 << 0;
   addFlit(net_packet_list, Flit::HEAD, 6, 1, Router::Id(0,2), Router::Id(0,20), 1, &output_endpoints);

   // Tail Flit - 1
   addFlit(net_packet_list, Flit::TAIL, 7, 1, Router::Id(0,2), Router::Id(0,20));
   
   // Body Flit - 0
   addFlit(net_packet_list, Flit::BODY, 7, 1, Router::Id(0,1), Router::Id(0,20));

   // Tail Flit - 0
   addFlit(net_packet_list, Flit::TAIL, 10, 1, Router::Id(0,1), Router::Id(0,20));

   return net_packet_list;
}

void addFlit(list<NetPacket*>& net_packet_list,
      Flit::Type flit_type, UInt64 time, SInt32 num_phits,
      Router::Id sender_router_id, Router::Id receiver_router_id,
      SInt32 num_output_endpoints, UnstructuredBuffer* output_endpoints_ptr)
{
   Flit* flit;
   if (flit_type & Flit::HEAD)
   {
      HeadFlit* head_flit = new HeadFlit(1, num_phits, INVALID_CORE_ID, INVALID_CORE_ID);
      head_flit->_type = flit_type;
           
      // Add Output Endpoints
      assert(output_endpoints_ptr);
      UnstructuredBuffer& output_endpoints = *output_endpoints_ptr;

      vector<Channel::Endpoint> output_endpoint_vec;
      for (SInt32 i = 0; i < num_output_endpoints; i++)
      {
         SInt32 channel_id;
         SInt32 index;
         output_endpoints >> channel_id;
         output_endpoints >> index;
         output_endpoint_vec.push_back(Channel::Endpoint(channel_id, index));
      }
      head_flit->_output_endpoint_list = new vector<Channel::Endpoint>(output_endpoint_vec);
      flit = head_flit;
   }
   else
   {
      flit = new Flit(flit_type, num_phits, INVALID_CORE_ID, INVALID_CORE_ID);
   }

   flit->_normalized_time = time;
   flit->_normalized_time_at_entry = time;
   flit->_sender_router_index = sender_router_id._index;
   flit->_receiver_router_index = receiver_router_id._index;
   
   NetPacket* net_packet = new NetPacket(time, USER_2,
         sender_router_id._core_id, receiver_router_id._core_id,
         sizeof(*flit), flit,
         false);
   flit->_net_packet = net_packet;

   net_packet_list.push_back(net_packet);
}

void addCreditMsg(list<NetPacket*>& net_packet_list,
      UInt64 time, SInt32 num_credits,
      Router::Id sender_router_id, Router::Id receiver_router_id)
{
   CreditMsg* credit_msg = new CreditMsg(time, num_credits);
   credit_msg->_sender_router_index = sender_router_id._index;
   credit_msg->_receiver_router_index = receiver_router_id._index;

   NetPacket* net_packet = new NetPacket(time, USER_2,
         sender_router_id._core_id, receiver_router_id._core_id,
         sizeof(*credit_msg), credit_msg,
         false);

   net_packet_list.push_back(net_packet);
}

void addOnOffMsg(list<NetPacket*>& net_packet_list,
      UInt64 time, bool on_off_status,
      Router::Id sender_router_id, Router::Id receiver_router_id)
{
   OnOffMsg* on_off_msg = new OnOffMsg(time, on_off_status);
   on_off_msg->_sender_router_index = sender_router_id._index;
   on_off_msg->_receiver_router_index = receiver_router_id._index;

   NetPacket* net_packet = new NetPacket(time, USER_2,
         sender_router_id._core_id, receiver_router_id._core_id,
         sizeof(*on_off_msg), on_off_msg,
         false);

   net_packet_list.push_back(net_packet);
}

void destroyNetPacketList(list<NetPacket*>& net_packet_list)
{
   for (list<NetPacket*>::iterator it = net_packet_list.begin(); it != net_packet_list.end(); it ++)
      destroyNetPacket(*it);
}

void destroyNetPacket(NetPacket* net_packet)
{
   net_packet->release();
}
