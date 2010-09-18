#include <math.h>
#include <vector>
using namespace std;

#include "network_model_atac_cluster.h"
#include "simulator.h"
#include "core_manager.h"
#include "core.h"
#include "config.h"
#include "log.h"
#include "memory_manager_base.h"
#include "packet_type.h"
#include "clock_converter.h"
#include "utils.h"
#include "queue_model_history_list.h"
#include "queue_model_history_tree.h"

UInt32 NetworkModelAtacCluster::m_total_cores = 0;
SInt32 NetworkModelAtacCluster::m_cluster_size = 0;
SInt32 NetworkModelAtacCluster::m_sqrt_cluster_size = 0;
UInt32 NetworkModelAtacCluster::m_num_clusters = 0;
SInt32 NetworkModelAtacCluster::m_mesh_width = 0;
SInt32 NetworkModelAtacCluster::m_mesh_height = 0;

NetworkModelAtacCluster::NetworkModelAtacCluster(Network *net, SInt32 network_id):
   NetworkModel(net, network_id),
   m_enabled(false)
{
   m_core_id = getNetwork()->getCore()->getId();
  
   // Initialize ENet, ONet and BNet parameters
   createANetRouterAndLinkModels();

   // Optical Hub created only on one of the cores in the cluster
   createOpticalHub();

   // Performance Counters
   initializePerformanceCounters();
}

NetworkModelAtacCluster::~NetworkModelAtacCluster()
{
   // Destroy the Optical hub created on one of the cores in the cluster
   destroyOpticalHub();

   // Destroy the Link Models
   destroyANetRouterAndLinkModels();
}

void
NetworkModelAtacCluster::initializeANetTopologyParams()
{
   // Initialize m_total_cores, m_cluster_size, m_sqrt_cluster_size, m_mesh_width, m_mesh_height, m_num_clusters
   m_total_cores = Config::getSingleton()->getTotalCores();

   try
   {
      m_cluster_size = Sim()->getCfg()->getInt("network/atac_cluster/cluster_size");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Error reading atac cluster size");
   }

   // Cluster Size
   m_sqrt_cluster_size = (SInt32) floor(sqrt(m_cluster_size));
   LOG_ASSERT_ERROR(m_cluster_size == (m_sqrt_cluster_size * m_sqrt_cluster_size),
         "Cluster Size(%i) must be a perfect square", m_cluster_size);

   // Calculations with an electrical mesh
   m_mesh_width = (SInt32) floor(sqrt(m_total_cores));
   m_mesh_height = (SInt32) ceil(1.0 * m_total_cores / m_mesh_width);
   LOG_ASSERT_ERROR(m_mesh_width % m_sqrt_cluster_size == 0, \
         "Mesh Width(%i) must be a multiple of sqrt_cluster_size(%i)", \
         m_mesh_width, m_sqrt_cluster_size);
   LOG_ASSERT_ERROR(m_mesh_height == (m_mesh_width + 1),
         "Mesh Width(%i), Mesh Height(%i)", m_mesh_width, m_mesh_height);
   LOG_ASSERT_ERROR((m_mesh_width * m_mesh_height) == (SInt32) m_total_cores,
         "Mesh Width(%i), Mesh Height(%i), Core Count(%i)", m_mesh_width, m_mesh_height, m_total_cores);

   // Number of Clusters
   m_num_clusters = (m_mesh_width / m_sqrt_cluster_size) * ceil(1.0 * m_mesh_height / m_sqrt_cluster_size);
}

void
NetworkModelAtacCluster::createANetRouterAndLinkModels()
{
   // 1) Initialize Frequency Parameters
   // Everything is normalized to the frequency of the ENet - the frequency of the network
   // is taken as the frequency of the ENet
   try
   {
      m_gather_network_frequency = Sim()->getCfg()->getFloat("network/atac_cluster/enet/frequency");
      m_optical_network_frequency = Sim()->getCfg()->getFloat("network/atac_cluster/onet/frequency");
      m_scatter_network_frequency = Sim()->getCfg()->getFloat("network/atac_cluster/bnet/frequency");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read ANet frequency parameters from the cfg file");
   }
   
   // Currently assume gather_network and scatter_network have the same frequency
   LOG_ASSERT_ERROR(m_gather_network_frequency == m_scatter_network_frequency, \
         "Currently, ENet and BNet should have the same frequency, \
         specified frequencies: ENet(%f), BNet(%f)", \
         m_gather_network_frequency, m_scatter_network_frequency);

   // 2) Initialize Bandwidth Parameters
   try
   {
      m_gather_network_link_width = Sim()->getCfg()->getInt("network/atac_cluster/enet/link/width");
      m_optical_network_link_width = Sim()->getCfg()->getInt("network/atac_cluster/onet/link/width");
      m_scatter_network_link_width = Sim()->getCfg()->getInt("network/atac_cluster/bnet/link/width");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read ANet link width parameters from the cfg file");
   }
   
   m_gather_network_bandwidth = static_cast<double>(m_gather_network_link_width);
   m_optical_network_bandwidth = static_cast<double>(m_optical_network_link_width) * (m_optical_network_frequency / m_gather_network_frequency);
   m_scatter_network_bandwidth = static_cast<double>(m_scatter_network_link_width) * (m_scatter_network_frequency / m_gather_network_frequency);
   m_effective_anet_bandwidth = getMin<double>(m_gather_network_bandwidth, m_optical_network_bandwidth, m_scatter_network_bandwidth);

   // 3) Initialize Latency Parameters
   volatile double gather_network_link_length;
   UInt32 gather_network_router_delay = 0;
   UInt32 num_flits_per_output_buffer_gather_network_router = 0;
   volatile double waveguide_length;
   volatile double scatter_network_link_length;
   try
   {
      // Router Delay of the electrical mesh network - Specified in clock cycles
      gather_network_router_delay = Sim()->getCfg()->getInt("network/atac_cluster/enet/router/delay");
      // Router Power Modeling
      num_flits_per_output_buffer_gather_network_router = Sim()->getCfg()->getInt("network/atac_cluster/enet/router/num_flits_per_port_buffer");
      // Length of a gather delay link
      gather_network_link_length = Sim()->getCfg()->getFloat("network/atac_cluster/enet/link/length");

      // Waveguide length of the optical network
      waveguide_length = Sim()->getCfg()->getFloat("network/atac_cluster/onet/link/length");
     
      // Length of a scatter network link (for the purpose of modeling power) 
      scatter_network_link_length = Sim()->getCfg()->getFloat("network/atac_cluster/bnet/link/length");
      // Specified in terms of clock cycles where the clock frequency = scatter_network_frequency
      // Delay is the delay of one scatter_network network. So, you need to pay just this delay when
      // going from the receiver hub to the receiver core
      m_scatter_network_delay = Sim()->getCfg()->getInt("network/atac_cluster/bnet/network_delay");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read ANet link delay and length parameters from the cfg file");
   }

   // 4) Link type parameters
   try
   {
      m_gather_network_link_type = Sim()->getCfg()->getString("network/atac_cluster/enet/link/type");
      m_scatter_network_link_type = Sim()->getCfg()->getString("network/atac_cluster/bnet/link/type");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read ANet link type parameters from the cfg file");
   }

   // Optical Network Link Models
   m_optical_network_link_model = new OpticalNetworkLinkModel(m_optical_network_frequency, \
         waveguide_length, \
         m_optical_network_link_width);
   m_optical_network_link_delay = (UInt64) (ceil( ((double) m_optical_network_link_model->getDelay()) / \
                      (m_optical_network_frequency / m_gather_network_frequency) ) );

   // Gather Network Router and Link Models
   m_num_gather_network_router_ports = 5;
   m_num_hops_sender_core_to_sender_hub = (m_sqrt_cluster_size/2 + 1);

   m_gather_network_router_model = ElectricalNetworkRouterModel::create(m_num_gather_network_router_ports, 
         m_num_gather_network_router_ports, \
         num_flits_per_output_buffer_gather_network_router, m_gather_network_link_width);
   m_gather_network_link_model = ElectricalNetworkLinkModel::create(m_gather_network_link_type, \
         m_gather_network_frequency, \
         gather_network_link_length, \
         m_gather_network_link_width);
   // Specified in terms of clock cycles where the clock frequency = gather_network frequency
   // Delay is the delay of one gather_network link
   // There may be multiple gather_network links from the core to the sending hub
   UInt64 gather_network_link_delay = m_gather_network_link_model->getDelay();
   // gather_network_link_delay is already initialized
   m_gather_network_delay = (gather_network_link_delay + gather_network_router_delay) * m_num_hops_sender_core_to_sender_hub;

   // Scatter Network link models
   // FIXME: Currently, the BNet network power is modeled using multiple scatter_network links.
   m_scatter_network_link_model = ElectricalNetworkLinkModel::create(m_scatter_network_link_type, \
         m_scatter_network_frequency, \
         scatter_network_link_length, \
         m_scatter_network_link_width);
   // m_scatter_network_delay is already initialized. Conversion needed here
   m_scatter_network_delay = (UInt64) (ceil( ((double) m_scatter_network_delay) / \
                      (m_scatter_network_frequency / m_gather_network_frequency) ) );

   initializeActivityCounters();
}

void
NetworkModelAtacCluster::initializeActivityCounters()
{
   // Initialize Activity Counters
   m_gather_network_router_switch_allocator_traversals = 0;
   m_gather_network_router_crossbar_traversals = 0;
   m_gather_network_link_traversals = 0;
   m_optical_network_link_traversals = 0;
   m_scatter_network_link_traversals = 0;
}

void
NetworkModelAtacCluster::destroyANetRouterAndLinkModels()
{
   delete m_gather_network_router_model;
   delete m_gather_network_link_model;
   delete m_optical_network_link_model;
   delete m_scatter_network_link_model;
}

void
NetworkModelAtacCluster::createOpticalHub()
{
   try
   {
      m_num_scatter_networks_per_cluster = Sim()->getCfg()->getInt("network/atac_cluster/num_bnets_per_cluster");
      m_queue_model_enabled = Sim()->getCfg()->getBool("network/atac_cluster/queue_model/enabled");
      m_queue_model_type = Sim()->getCfg()->getString("network/atac_cluster/queue_model/type");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read cluster and queue model parameters from the cfg file");
   }

   createQueueModels();
}

void
NetworkModelAtacCluster::destroyOpticalHub()
{
   destroyQueueModels();
}

void
NetworkModelAtacCluster::createQueueModels()
{
   if (m_queue_model_enabled && (m_core_id == getCoreIDWithOpticalHub(getClusterID(m_core_id))))
   {
      // I am one of the cores with an optical hub

      UInt64 min_processing_time = 1;
     
      // Create Sending Hub 
      m_sender_hub_queue_model = QueueModel::create(m_queue_model_type, min_processing_time);
      m_total_sender_hub_contention_delay = 0;
      m_total_sender_hub_packets = 0;
      m_total_buffered_sender_hub_packets = 0; 

      // Create Receiving Hub 
      m_receiver_hub_queue_models = new QueueModel*[m_num_scatter_networks_per_cluster];
      m_total_receiver_hub_contention_delay = new UInt64[m_num_scatter_networks_per_cluster];
      m_total_receiver_hub_packets = new UInt64[m_num_scatter_networks_per_cluster];
      m_total_buffered_receiver_hub_packets = new UInt64[m_num_scatter_networks_per_cluster];
      
      for (SInt32 i = 0; i < (SInt32) m_num_scatter_networks_per_cluster; i++)
      {
         m_receiver_hub_queue_models[i] = QueueModel::create(m_queue_model_type, min_processing_time);
         m_total_receiver_hub_contention_delay[i] = 0;
         m_total_receiver_hub_packets[i] = 0;
         m_total_buffered_receiver_hub_packets[i] = 0;
      }

#ifdef TRACK_UTILIZATION
      initializeUtilizationCounters();
#endif
   }
   else
   {
      m_sender_hub_queue_model = (QueueModel*) NULL;
      m_receiver_hub_queue_models = (QueueModel**) NULL;
   }
}

void
NetworkModelAtacCluster::destroyQueueModels()
{
   if ((m_queue_model_enabled) && (m_core_id == getCoreIDWithOpticalHub(getClusterID(m_core_id))))
   {
      delete m_sender_hub_queue_model;
      
      for (SInt32 i = 0; i < (SInt32) m_num_scatter_networks_per_cluster; i++)
         delete m_receiver_hub_queue_models[i];
      delete m_receiver_hub_queue_models;

      delete m_total_receiver_hub_contention_delay;
      delete m_total_receiver_hub_packets;

#ifdef TRACK_UTILIZATION
      destroyUtilizationCounters();
#endif
   }
}

void
NetworkModelAtacCluster::resetQueueModels()
{
   destroyQueueModels();
   createQueueModels();
}

void
NetworkModelAtacCluster::initializePerformanceCounters()
{
   m_total_bytes_received = 0;
   m_total_packets_received = 0;
   m_total_contention_delay = 0;
   m_total_packet_latency = 0;
}

UInt64 
NetworkModelAtacCluster::computeProcessingTime(UInt32 pkt_length, volatile double bandwidth)
{
   return static_cast<UInt64>(ceil(static_cast<double>(pkt_length * 8) / bandwidth));
}

core_id_t
NetworkModelAtacCluster::getRequester(const NetPacket& pkt)
{
   core_id_t requester = INVALID_CORE_ID;

   if ((pkt.type == SHARED_MEM_1) || (pkt.type == SHARED_MEM_2))
      requester = getNetwork()->getCore()->getMemoryManager()->getShmemRequester(pkt.data);
   else // Other Packet types
      requester = pkt.sender;

   LOG_ASSERT_ERROR((requester >= 0) && (requester < (core_id_t) Config::getSingleton()->getTotalCores()),
         "requester(%i)", requester);
   
   return requester;
}

UInt64
NetworkModelAtacCluster::getHubQueueDelay(NetworkComponentType hub_type, SInt32 sender_cluster_id, SInt32 cluster_id, UInt64 pkt_time, const NetPacket& pkt)
{
   core_id_t core_id_with_optical_hub = getCoreIDWithOpticalHub(cluster_id);
   Core* core = Sim()->getCoreManager()->getCoreFromID(core_id_with_optical_hub);
   NetworkModelAtacCluster* network_model = (NetworkModelAtacCluster*) core->getNetwork()->getNetworkModelFromPacketType(pkt.type);

   return network_model->computeHubQueueDelay(hub_type, sender_cluster_id, pkt_time, pkt);
}

UInt64
NetworkModelAtacCluster::computeHubQueueDelay(NetworkComponentType hub_type, SInt32 sender_cluster_id, UInt64 pkt_time, const NetPacket& pkt)
{
   core_id_t requester = getRequester(pkt);
   UInt32 pkt_length = getNetwork()->getModeledLength(pkt);

   if ( (!m_enabled) || (!m_queue_model_enabled) || (requester >= (core_id_t) Config::getSingleton()->getApplicationCores()) || (!getNetwork()->getCore()->getMemoryManager()->isModeled(pkt.data)) )
      return 0;

   assert(m_core_id == getCoreIDWithOpticalHub(getClusterID(m_core_id)));
   assert(m_sender_hub_queue_model);
   assert(m_receiver_hub_queue_models);

   switch (hub_type)
   {
      case SENDER_HUB:
         {
            ScopedLock sl(m_sender_hub_lock);

            // Convert from gather network clock to optical network clock
            UInt64 optical_network_pkt_time = convertCycleCount(pkt_time, m_gather_network_frequency, m_optical_network_frequency);
            
            UInt64 processing_time = computeProcessingTime(pkt_length, (volatile double) m_optical_network_link_width);

            LOG_ASSERT_ERROR(sender_cluster_id == getClusterID(m_core_id), \
                  "sender_cluster_id(%i), curr_cluster_id(%i)", \
                  sender_cluster_id, getClusterID(m_core_id));
            UInt64 sender_hub_queue_delay = m_sender_hub_queue_model->computeQueueDelay(optical_network_pkt_time, processing_time);

            // Update Utilization Counters
#ifdef TRACK_UTILIZATION
            updateUtilization(SENDER_HUB, -1, optical_network_pkt_time + sender_hub_queue_delay, processing_time);
#endif

            // Performance Counters
            m_total_sender_hub_contention_delay += sender_hub_queue_delay;
            if (sender_hub_queue_delay != 0)
               m_total_buffered_sender_hub_packets ++;
            m_total_sender_hub_packets ++;

            // Also convert from optical network clock to gather network clock
            return convertCycleCount(sender_hub_queue_delay, m_optical_network_frequency, m_gather_network_frequency);
         }

      case RECEIVER_HUB:
         {
            ScopedLock sl(m_receiver_hub_lock);
           
            // Convert from gather network clock to scatter network clock
            UInt64 scatter_network_pkt_time = convertCycleCount(pkt_time, m_gather_network_frequency, m_scatter_network_frequency);

            UInt64 processing_time = computeProcessingTime(pkt_length, (volatile double) m_scatter_network_link_width);

            // Assume the broadcast networks are statically divided up among the senders
            SInt32 scatter_network_num = sender_cluster_id % m_num_scatter_networks_per_cluster;
            UInt64 receiver_hub_queue_delay = m_receiver_hub_queue_models[scatter_network_num]->computeQueueDelay(scatter_network_pkt_time, processing_time);

            // Update Utilization Counters
#ifdef TRACK_UTILIZATION
            updateUtilization(RECEIVER_HUB, scatter_network_num, scatter_network_pkt_time + receiver_hub_queue_delay, processing_time);
#endif

            // Performance Counters
            m_total_receiver_hub_contention_delay[scatter_network_num] += receiver_hub_queue_delay;
            if (receiver_hub_queue_delay != 0)
               m_total_buffered_receiver_hub_packets[scatter_network_num] ++;
            m_total_receiver_hub_packets[scatter_network_num] ++;
            
            // Also convert from scatter network clock to gather network clock
            return convertCycleCount(receiver_hub_queue_delay, m_scatter_network_frequency, m_gather_network_frequency);
         }

      default:
         LOG_PRINT_ERROR("Network Component(%u) not a Hub", hub_type);
         return 0;
   }
}

UInt32
NetworkModelAtacCluster::computeAction(const NetPacket& pkt)
{
   // If this endpoint is a hub, FORWARD it, else RECEIVE it
   // FORWARD may mean forward it to mutiple recipients too depending
   // on the routing function
   if ((pkt.specific == SENDER_HUB) || (pkt.specific == RECEIVER_HUB))
   {
      return RoutingAction::FORWARD;
   }
   else if (pkt.specific == RECEIVER_CORE)
   {
      LOG_ASSERT_ERROR((pkt.receiver == NetPacket::BROADCAST) || (pkt.receiver == m_core_id),
            "pkt.receiver(%i)", pkt.receiver);
      return RoutingAction::RECEIVE;
   }
   else
   {
      LOG_PRINT_ERROR("Unhandled sub network type(%u)", pkt.specific);
      return RoutingAction::DROP;
   }
}

void 
NetworkModelAtacCluster::routePacket(const NetPacket &pkt, std::vector<Hop> &nextHops)
{
   ScopedLock sl(m_lock);

   if (Config::getSingleton()->getProcessCount() == 1)
   {
      // All the energies and delays are computed at source
      if (pkt.receiver == NetPacket::BROADCAST)
      {
         updateDynamicEnergy(GATHER_NETWORK, pkt);
         updateDynamicEnergy(OPTICAL_NETWORK, pkt);
         
         UInt64 sender_hub_queue_delay = getHubQueueDelay(SENDER_HUB, \
               getClusterID(pkt.sender), \
               getClusterID(pkt.sender), \
               pkt.time + m_gather_network_delay, \
               pkt);
         UInt64 latency_sender_core_to_receiver_hub = m_gather_network_delay + \
                                                      sender_hub_queue_delay + \
                                                      m_optical_network_link_delay;

         UInt64 receiver_hub_queue_delay[m_num_clusters];
         for (SInt32 i = 0; i < (SInt32) m_num_clusters; i++)
         {
            updateDynamicEnergy(SCATTER_NETWORK, pkt);
            
            receiver_hub_queue_delay[i] = getHubQueueDelay(RECEIVER_HUB, \
                  getClusterID(pkt.sender), \
                  i, \
                  pkt.time + latency_sender_core_to_receiver_hub, \
                  pkt);
         }

         for (core_id_t i = 0; i < (core_id_t) m_total_cores; i++)
         {
            UInt64 latency_receiver_hub_to_receiver_core = receiver_hub_queue_delay[getClusterID(i)] + \
                                                           m_scatter_network_delay;

            UInt64 total_latency = latency_sender_core_to_receiver_hub + \
                                   latency_receiver_hub_to_receiver_core;

            Hop h;
            h.next_dest = i;
            h.final_dest = NetPacket::BROADCAST;
            h.specific = RECEIVER_CORE;
            h.time = pkt.time + total_latency;
            
            nextHops.push_back(h);
         }
      }
      else // if (pkt.receiver != NetPacket::BROADCAST)
      {
         // Right now, it is a unicast/multicast
         LOG_ASSERT_ERROR(pkt.receiver < (core_id_t) m_total_cores, \
               "Got invalid receiver ID = %i", pkt.receiver);

         UInt64 total_latency;
         if (pkt.sender == pkt.receiver)
         {
            total_latency = 0;
         }
         else
         {
            updateDynamicEnergy(GATHER_NETWORK, pkt);

            UInt64 latency_sender_core_to_receiver_hub;
            if (getClusterID(pkt.sender) == getClusterID(pkt.receiver))
            {
               latency_sender_core_to_receiver_hub = m_gather_network_delay;
            }
            else
            {
               updateDynamicEnergy(OPTICAL_NETWORK, pkt);
               
               UInt64 sender_hub_queue_delay = getHubQueueDelay(SENDER_HUB, \
                     getClusterID(pkt.sender), \
                     getClusterID(pkt.sender), \
                     pkt.time + m_gather_network_delay, \
                     pkt);
               latency_sender_core_to_receiver_hub = m_gather_network_delay + \
                                                     sender_hub_queue_delay + \
                                                     m_optical_network_link_delay;
            }

            updateDynamicEnergy(SCATTER_NETWORK, pkt);

            UInt64 receiver_hub_queue_delay = getHubQueueDelay(RECEIVER_HUB, \
                  getClusterID(pkt.sender), \
                  getClusterID(pkt.receiver), \
                  pkt.time + latency_sender_core_to_receiver_hub, \
                  pkt);
            UInt64 latency_receiver_hub_to_receiver_core = receiver_hub_queue_delay + \
                                                           m_scatter_network_delay;

            total_latency = latency_sender_core_to_receiver_hub + \
                            latency_receiver_hub_to_receiver_core;
         }

         Hop h;
         h.next_dest = pkt.receiver;
         h.final_dest = pkt.receiver;
         h.specific = RECEIVER_CORE;
         h.time = pkt.time + total_latency;
         
         nextHops.push_back(h);
      }
   }
   else // Config::getSingleton()->getProcessCount() > 1
   {
      // Process Count > 1
      // Let me have at least 2 hops within the same process
      // and 1 hop between processes
      if (pkt.receiver == NetPacket::BROADCAST)
      {
         if (pkt.specific == SENDER_HUB)
         {
            // Sender Hub
            // 1) Send it to all the other Hubs on the chip
            // 2) Broadcast it to all the cores within the same cluster
            // Compute:
            // 1) Sender Hub queue delay
            
            // Optical Link Traversal
            updateDynamicEnergy(OPTICAL_NETWORK, pkt);

            UInt64 sender_hub_queue_delay = computeHubQueueDelay(SENDER_HUB, \
                  getClusterID(pkt.sender), \
                  pkt.time, \
                  pkt);
            
            // First, send it to all the other hubs
            for (SInt32 i = 0; i < (SInt32) m_num_clusters; i++)
            {
               if (i != getClusterID(m_core_id))
               {
                  Hop h;
                  h.next_dest = getCoreIDWithOpticalHub(i);
                  h.final_dest = NetPacket::BROADCAST;
                  h.specific = RECEIVER_HUB;
                  h.time = pkt.time + sender_hub_queue_delay + m_optical_network_link_delay;

                  nextHops.push_back(h);
               }
            }

            // Broadcast Network Traversal
            updateDynamicEnergy(SCATTER_NETWORK, pkt);

            // Second, send it to all the cores within the same cluster
            // We need to compute receiver hub queue delay here
            UInt64 receiver_hub_queue_delay = computeHubQueueDelay(RECEIVER_HUB, \
                  getClusterID(pkt.sender), \
                  pkt.time, \
                  pkt);

            vector<core_id_t> core_id_list_in_cluster;
            getCoreIDListInCluster(getClusterID(m_core_id), core_id_list_in_cluster);
            for (vector<core_id_t>::iterator core_it = core_id_list_in_cluster.begin(); \
                  core_it != core_id_list_in_cluster.end(); core_it++)
            {
               Hop h;
               h.next_dest = *core_it;
               h.final_dest = NetPacket::BROADCAST;
               h.specific = RECEIVER_CORE;
               h.time = pkt.time + receiver_hub_queue_delay + m_scatter_network_delay;

               nextHops.push_back(h);
            }
         }
         
         else if (pkt.specific == RECEIVER_HUB)
         {
            // Receiver Hub
            
            // Broadcast Network Traversal
            updateDynamicEnergy(SCATTER_NETWORK, pkt);

            // Send it to all the cores within the same cluster
            // We need to compute receiver hub queue delay here
            UInt64 receiver_hub_queue_delay = computeHubQueueDelay(RECEIVER_HUB, \
                  getClusterID(pkt.sender), \
                  pkt.time, \
                  pkt);

            vector<core_id_t> core_id_list_in_cluster;
            getCoreIDListInCluster(getClusterID(m_core_id), core_id_list_in_cluster);
            for (vector<core_id_t>::iterator core_it = core_id_list_in_cluster.begin(); \
                  core_it != core_id_list_in_cluster.end(); core_it++)
            {
               Hop h;
               h.next_dest = *core_it;
               h.final_dest = NetPacket::BROADCAST;
               h.specific = RECEIVER_CORE;
               h.time = pkt.time + receiver_hub_queue_delay + m_scatter_network_delay;

               nextHops.push_back(h);
            }
         }

         else if (pkt.specific == INVALID_COMPONENT)
         {
            // Must be the sender. pkt.specific is not set
            LOG_ASSERT_ERROR(pkt.sender == m_core_id, "pkt.sender(%i), m_core_id(%i)",
                  pkt.sender, m_core_id);
               
            // Sender Cluster Electrical Network(ENet) traversal
            updateDynamicEnergy(GATHER_NETWORK, pkt);

            // I am the sender of the broadcast packet
            // I dont have an associated optical hub
            // Send it to the core having the hub associated with the current cluster
            Hop h;
            h.next_dest = getCoreIDWithOpticalHub(getClusterID(m_core_id));
            h.final_dest = NetPacket::BROADCAST;
            h.specific = SENDER_HUB;
            h.time = pkt.time + m_gather_network_delay;

            nextHops.push_back(h);
         }

         else // pkt.specific is not one of the known values
         {
            LOG_PRINT_ERROR("Routing Function not defined: pkt.sender(%i), \
                  pkt.receiver(%i), pkt.specific(%u), pkt.time(%llu)",
                  pkt.sender, pkt.receiver, pkt.specific, pkt.time);
         }
      }

      else // pkt.receiver != NetPacket::BROADCAST
      {
         // Unicasted/Multicasted packet
         LOG_ASSERT_ERROR(pkt.receiver < (core_id_t) m_total_cores, \
               "Got invalid receiver ID = %i", pkt.receiver);

         UInt64 hop_latency = 0;
         core_id_t next_dest = INVALID_CORE_ID;
         UInt32 next_component = 0;

         if (pkt.specific == SENDER_HUB)
         {
            // Is receiver in the same cluster
            if (getClusterID(pkt.sender) == getClusterID(pkt.receiver))
            {
               // BNet traversal
               updateDynamicEnergy(SCATTER_NETWORK, pkt);

               // Yes, calculate the receiver hub queue delay
               UInt64 receiver_hub_queue_delay = computeHubQueueDelay(RECEIVER_HUB, \
                   getClusterID(pkt.sender), \
                   pkt.time, \
                   pkt);

               hop_latency = (receiver_hub_queue_delay + m_scatter_network_delay);
               next_dest = pkt.receiver;
               next_component = RECEIVER_CORE;
            }
            else // getClusterID(pkt.sender) != getClusterID(pkt.receiver)
            {
               // ONet traversal
               updateDynamicEnergy(OPTICAL_NETWORK, pkt);

               // Receiving cluster different from sending cluster
               UInt64 sender_hub_queue_delay = computeHubQueueDelay(SENDER_HUB, \
                    getClusterID(pkt.sender), \
                    pkt.time, \
                    pkt);

               hop_latency = (sender_hub_queue_delay + m_optical_network_link_delay);
               next_dest = getCoreIDWithOpticalHub(getClusterID(pkt.receiver));
               next_component = RECEIVER_HUB;
            }
         }

         else if (pkt.specific == RECEIVER_HUB)
         {
            // BNet traversal
            updateDynamicEnergy(SCATTER_NETWORK, pkt);

            // Receiver Hub
            // Forward the packet to the actual receiver
            UInt64 receiver_hub_queue_delay = computeHubQueueDelay(RECEIVER_HUB, \
                  getClusterID(pkt.sender), \
                  pkt.time, \
                  pkt);

            LOG_ASSERT_ERROR(getClusterID(m_core_id) == getClusterID(pkt.receiver), \
                  "m_core_id(%i), cluster(%u), pkt.receiver(%i), cluster(%u)", \
                  m_core_id, getClusterID(m_core_id), pkt.receiver, getClusterID(pkt.receiver));

            hop_latency = (receiver_hub_queue_delay + m_scatter_network_delay);
            next_dest = pkt.receiver;
            next_component = RECEIVER_CORE;
         }

         else if (pkt.specific == INVALID_COMPONENT)
         {
            // pkt.specific is not set. Must be the sender
            LOG_ASSERT_ERROR(pkt.sender == m_core_id, "pkt.sender(%i), m_core_id(%i)",
                  pkt.sender, m_core_id);
            
            if (pkt.sender == pkt.receiver)
            {
               hop_latency = 0;
               next_dest = pkt.receiver;
               next_component = RECEIVER_CORE;
            }
            else
            {
               // ENet traversal
               updateDynamicEnergy(GATHER_NETWORK, pkt);

               // Sending node - I am not optical hub
               hop_latency = m_gather_network_delay;
               next_dest = getCoreIDWithOpticalHub(getClusterID(pkt.sender));
               next_component = SENDER_HUB;
            }
         }
         
         else
         {
            LOG_PRINT_ERROR("Unrecognized Routing Function: pkt.sender(%i), \
                  pkt.receiver(%i), pkt.specific(%u), m_core_id(%i)",
                  pkt.sender, pkt.receiver, pkt.specific, m_core_id);
         }

         if (next_dest != INVALID_CORE_ID)
         {
            Hop h;
            h.next_dest = next_dest;
            h.final_dest = pkt.receiver;
            h.specific = next_component;
            h.time = pkt.time + hop_latency;

            nextHops.push_back(h);
         }

      } // if (pkt.receiver == NetPacket::BROADCAST)

   } // if (Config::getSingleton()->getProcessCount() == 1)
}

void
NetworkModelAtacCluster::processReceivedPacket(NetPacket& pkt)
{
   ScopedLock sl(m_lock);
 
   core_id_t requester = getRequester(pkt);
   if ( (!m_enabled) || (requester >= (core_id_t) Config::getSingleton()->getApplicationCores()) || (!getNetwork()->getCore()->getMemoryManager()->isModeled(pkt.data)) )
      return;

   UInt32 pkt_length = getNetwork()->getModeledLength(pkt);
   UInt64 processing_time = computeProcessingTime(pkt_length, m_effective_anet_bandwidth); 
   if (pkt.sender != pkt.receiver)
   {
      // Serialization Delay
      pkt.time += processing_time;
   }
   
   // Performance Counters
   m_total_packets_received ++;
   m_total_bytes_received += (UInt64) pkt_length;
   
   UInt64 packet_latency = pkt.time - pkt.start_time;

   // Increment Total Contention Delay
   if (pkt.sender == pkt.receiver)
   {
      m_total_contention_delay += 0;
      assert(packet_latency == 0);
   }
   else if (getClusterID(pkt.sender) == getClusterID(pkt.receiver))
   {
      m_total_contention_delay += (packet_latency - \
               (m_gather_network_delay + m_scatter_network_delay + processing_time));
   }
   else
   {
      m_total_contention_delay += (packet_latency - \
               (m_gather_network_delay + m_optical_network_link_delay + m_scatter_network_delay + processing_time));
   }

   // Increment Total Packet Latency
   m_total_packet_latency += packet_latency;
}

void
NetworkModelAtacCluster::outputHubSummary(ostream& out)
{
   out << " ATAC Cluster: " << endl;
   if ((m_queue_model_enabled) && (m_core_id == getCoreIDWithOpticalHub(getClusterID(m_core_id))))
   {
      // ONet
      if (m_total_sender_hub_packets > 0)
      {
         // Convert from optical network clock to global clock
         UInt64 total_sender_hub_contention_delay_in_ns = convertCycleCount( \
               m_total_sender_hub_contention_delay, \
               m_optical_network_frequency, 1.0);
         out << "    ONet Link Contention Delay (in ns): " << \
            ((float) total_sender_hub_contention_delay_in_ns) / m_total_sender_hub_packets << endl;
      }
      else
      {
         out << "    ONet Link Contention Delay (in ns): 0" << endl;
      }
      out << "    ONet Total Packets: " << m_total_sender_hub_packets << endl;
      out << "    ONet Total Buffered Packets: " << m_total_buffered_sender_hub_packets << endl;
    
      if ((m_queue_model_type == "history_list") || (m_queue_model_type == "history_tree"))
      {
         float sender_hub_utilization;
         double frac_analytical_model_used;
         if (m_queue_model_type == "history_list")
         {
            sender_hub_utilization = ((QueueModelHistoryList*) m_sender_hub_queue_model)->getQueueUtilization();
            frac_analytical_model_used = ((double) ((QueueModelHistoryList*) m_sender_hub_queue_model)->getTotalRequestsUsingAnalyticalModel()) / \
                                         ((QueueModelHistoryList*) m_sender_hub_queue_model)->getTotalRequests();
         }
         else // (m_queue_model_type == "history_tree")
         {
            sender_hub_utilization = ((QueueModelHistoryTree*) m_sender_hub_queue_model)->getQueueUtilization();
            frac_analytical_model_used = ((double) ((QueueModelHistoryTree*) m_sender_hub_queue_model)->getTotalRequestsUsingAnalyticalModel()) / \
                                         ((QueueModelHistoryTree*) m_sender_hub_queue_model)->getTotalRequests();
         }
         out << "    ONet Link Utilization(\%): " << sender_hub_utilization * 100 << endl; 
         out << "    ONet Analytical Model Used(\%): " << frac_analytical_model_used * 100 << endl;
      }

      // BNet
      for (UInt32 i = 0; i < m_num_scatter_networks_per_cluster; i++)
      {
         if (m_total_receiver_hub_packets[i] > 0)
         {
            // Convert from scatter network clock to global clock
            UInt64 total_receiver_hub_contention_delay_in_ns = convertCycleCount( \
                  m_total_receiver_hub_contention_delay[i], \
                  m_scatter_network_frequency, 1.0);
            out << "    BNet (" << i << ") Link Contention Delay (in ns): " \
               << ((float) total_receiver_hub_contention_delay_in_ns) / m_total_receiver_hub_packets[i] << endl;
         }
         else
         {
            out << "    BNet (" << i << ") Link Contention Delay (in ns): 0" << endl;
         }
         out << "    BNet (" << i << ") Total Packets: " << m_total_receiver_hub_packets[i] << endl;
         out << "    BNet (" << i << ") Total Buffered Packets: " << m_total_buffered_receiver_hub_packets[i] << endl;

         if ((m_queue_model_type == "history_list") || (m_queue_model_type == "history_tree"))
         {
            float receiver_hub_utilization;
            double frac_analytical_model_used;
            if (m_queue_model_type == "history_list")
            {
               receiver_hub_utilization = ((QueueModelHistoryList*) m_receiver_hub_queue_models[i])->getQueueUtilization();
               frac_analytical_model_used = ((double) ((QueueModelHistoryList*) m_receiver_hub_queue_models[i])->getTotalRequestsUsingAnalyticalModel()) / \
                                            ((QueueModelHistoryList*) m_receiver_hub_queue_models[i])->getTotalRequests();
            }
            else // (m_queue_model_type == "history_tree")
            {
               receiver_hub_utilization = ((QueueModelHistoryTree*) m_receiver_hub_queue_models[i])->getQueueUtilization();
               frac_analytical_model_used = ((double) ((QueueModelHistoryTree*) m_receiver_hub_queue_models[i])->getTotalRequestsUsingAnalyticalModel()) / \
                                            ((QueueModelHistoryTree*) m_receiver_hub_queue_models[i])->getTotalRequests();
            }
            out << "    BNet (" << i << ") Link Utilization(\%): " << receiver_hub_utilization * 100 << endl;
            out << "    BNet (" << i << ") Analytical Model Used(\%): " << frac_analytical_model_used * 100 << endl;
         }
      }

#ifdef TRACK_UTILIZATION
      outputUtilizationSummary();
#endif
   }
   else
   {
      out << "    ONet Link Contention Delay (in ns): NA" << endl;
      out << "    ONet Total Packets: NA" << endl;
      out << "    ONet Total Buffered Packets: NA" << endl;
      if ((m_queue_model_type == "history_list") || (m_queue_model_type == "history_tree"))
      {
         out << "    ONet Link Utilization(\%): NA" << endl;
         out << "    ONet Analytical Model Used(\%): NA" << endl;
      }
      
      for (UInt32 i = 0; i < m_num_scatter_networks_per_cluster; i++)
      {
         out << "    BNet (" << i << ") Link Contention Delay (in ns): NA" << endl;
         out << "    BNet (" << i << ") Total Packets: NA" << endl;
         out << "    BNet (" << i << ") Total Buffered Packets: NA" << endl;
         if ((m_queue_model_type == "history_list") || (m_queue_model_type == "history_tree"))
         {
            out << "    BNet (" << i << ") Link Utilization(\%): NA" << endl;
            out << "    BNet (" << i << ") Analytical Model Used(\%): NA" << endl;
         }
      }
   }
}

void
NetworkModelAtacCluster::outputSummary(ostream &out)
{
   out << "    bytes received: " << m_total_bytes_received << endl;
   out << "    packets received: " << m_total_packets_received << endl;
   if (m_total_packets_received > 0)
   {
      // Convert from gather network clock to global clock
      UInt64 total_contention_delay_in_ns = convertCycleCount(m_total_contention_delay, \
            m_gather_network_frequency, 1.0);
      out << "    average contention delay (in ns): " << \
         ((float) total_contention_delay_in_ns) / m_total_packets_received << endl;
      
      UInt64 total_packet_latency_in_ns = convertCycleCount(m_total_packet_latency, \
            m_gather_network_frequency, 1.0);
      out << "    average packet latency (in ns): " << \
         ((float) total_packet_latency_in_ns) / m_total_packets_received << endl;
   }
   else
   {
      out << "    average packet latency (in ns): 0" << endl;
   }

   outputHubSummary(out);
   outputPowerSummary(out);
}

void
NetworkModelAtacCluster::enable()
{
   m_enabled = true;
}

void
NetworkModelAtacCluster::disable()
{
   m_enabled = false;
}

void
NetworkModelAtacCluster::reset()
{
   // Performance Counters
   initializePerformanceCounters();

   // Queue Models
   resetQueueModels();

   // Activity Counters
   initializeActivityCounters();

   // Router & Link Models
   m_gather_network_router_model->resetCounters();
   m_gather_network_link_model->resetCounters();
   m_optical_network_link_model->resetCounters();
   m_scatter_network_link_model->resetCounters();
}

SInt32
NetworkModelAtacCluster::getClusterID(core_id_t core_id)
{
   // Consider a mesh formed by the clusters
   SInt32 cluster_mesh_width;
   cluster_mesh_width = m_mesh_width / m_sqrt_cluster_size;

   SInt32 core_x, core_y;
   core_x = core_id % m_mesh_width;
   core_y = core_id / m_mesh_width;

   SInt32 cluster_pos_x, cluster_pos_y;
   cluster_pos_x = core_x / m_sqrt_cluster_size;
   cluster_pos_y = core_y / m_sqrt_cluster_size;

   return (cluster_pos_y * cluster_mesh_width + cluster_pos_x);
}

core_id_t
NetworkModelAtacCluster::getCoreIDWithOpticalHub(SInt32 cluster_id)
{
   // Consider a mesh formed by the clusters
   SInt32 cluster_mesh_width;
   cluster_mesh_width = m_mesh_width / m_sqrt_cluster_size;

   SInt32 cluster_pos_x, cluster_pos_y;
   cluster_pos_x = cluster_id % cluster_mesh_width;
   cluster_pos_y = cluster_id / cluster_mesh_width;

   SInt32 optical_hub_x, optical_hub_y; 
   optical_hub_x = cluster_pos_x * m_sqrt_cluster_size;
   optical_hub_y = cluster_pos_y * m_sqrt_cluster_size;

   return (optical_hub_y * m_mesh_width + optical_hub_x);
}

void
NetworkModelAtacCluster::getCoreIDListInCluster(SInt32 cluster_id, vector<core_id_t>& core_id_list)
{
   SInt32 cluster_mesh_width;
   cluster_mesh_width = m_mesh_width / m_sqrt_cluster_size;

   SInt32 cluster_pos_x, cluster_pos_y;
   cluster_pos_x = cluster_id % cluster_mesh_width;
   cluster_pos_y = cluster_id / cluster_mesh_width;

   SInt32 optical_hub_x, optical_hub_y; 
   optical_hub_x = cluster_pos_x * m_sqrt_cluster_size;
   optical_hub_y = cluster_pos_y * m_sqrt_cluster_size;

   for (SInt32 i = optical_hub_x; i < optical_hub_x + m_sqrt_cluster_size; i++)
   {
      for (SInt32 j = optical_hub_y; j < optical_hub_y + m_sqrt_cluster_size; j++)
      {
         SInt32 core_id = j * m_mesh_width + i;
         if (core_id < (SInt32) Config::getSingleton()->getTotalCores())
            core_id_list.push_back(core_id);
      }
   }
}

pair<bool,SInt32>
NetworkModelAtacCluster::computeCoreCountConstraints(SInt32 core_count)
{
   // Same Calculations as Electrical Mesh Model
   SInt32 mesh_width = (SInt32) floor (sqrt(core_count));
   SInt32 mesh_height = (SInt32) ceil (1.0 * core_count / mesh_width);

   assert(core_count <= mesh_width * mesh_height);
   assert(core_count > (mesh_width - 1) * mesh_height);
   assert(core_count > mesh_width * (mesh_height - 1));

   return make_pair(true,mesh_height * mesh_width);
}

pair<bool, vector<core_id_t> >
NetworkModelAtacCluster::computeMemoryControllerPositions(SInt32 num_memory_controllers, SInt32 core_count)
{
   // Initialize the topology parameters in case called by an external model
   initializeANetTopologyParams();

   // Initialization should be done by now
   // Get the cluster size, Get the number of clusters
   // Here we only include complete clusters, we dont include the incomplete clusters
   SInt32 num_clusters = (m_mesh_width / m_sqrt_cluster_size) * (m_mesh_width / m_sqrt_cluster_size);
   LOG_ASSERT_ERROR(num_memory_controllers <= num_clusters,
         "num_memory_controllers(%i), num_clusters(%i)", num_memory_controllers, num_clusters);

   vector<core_id_t> core_id_list_with_memory_controllers;
   for (SInt32 i = 0; i < num_memory_controllers; i++)
   {
      core_id_list_with_memory_controllers.push_back(getCoreIDWithOpticalHub(i));
   }

   return (make_pair(true, core_id_list_with_memory_controllers));
}

pair<bool, vector<Config::CoreList> >
NetworkModelAtacCluster::computeProcessToCoreMapping()
{
   // Initialize m_total_cores, m_cluster_size, m_sqrt_cluster_size, m_mesh_width, m_mesh_height, m_num_clusters
   initializeANetTopologyParams();

   UInt32 process_count = Config::getSingleton()->getProcessCount();
   vector<Config::CoreList> process_to_core_mapping(process_count);
  
   LOG_ASSERT_WARNING(m_num_clusters >= process_count,
        "Number of Clusters(%u) < Total Process in Simulation(%u)",
        m_num_clusters, process_count);
        
   UInt32 process_num = 0;
   for (UInt32 i = 0; i < m_num_clusters; i++)
   {
      Config::CoreList core_id_list;
      Config::CoreList::iterator core_it;
      getCoreIDListInCluster(i, core_id_list);
      for (core_it = core_id_list.begin(); core_it != core_id_list.end(); core_it ++)
      {
         process_to_core_mapping[process_num].push_back(*core_it);
      }
      process_num = (process_num + 1) % process_count;
   }

   return (make_pair(true, process_to_core_mapping));
}

void
NetworkModelAtacCluster::updateDynamicEnergy(SubNetworkType sub_net_type, const NetPacket& pkt)
{
   // This function calls the power models as well as update power counters
   core_id_t requester = getRequester(pkt);
   if ( (!m_enabled) || (requester >= ((core_id_t) Config::getSingleton()->getApplicationCores())) || (!getNetwork()->getCore()->getMemoryManager()->isModeled(pkt.data)) )
      return;

   // TODO: Make these models more detailed later - Compute the exact number of bit flips
   UInt32 pkt_length = getNetwork()->getModeledLength(pkt);

   switch(sub_net_type)
   {
      case GATHER_NETWORK:
         // We basically look at a mesh and compute the average number of links traversed
         if (Config::getSingleton()->getEnablePowerModeling())
         {
            // Update Dynamic Energy of router (switch allocator + crossbar)
            m_gather_network_router_model->updateDynamicEnergySwitchAllocator(m_num_gather_network_router_ports/2, \
                  m_num_hops_sender_core_to_sender_hub);
            m_gather_network_router_model->updateDynamicEnergyClock(m_num_hops_sender_core_to_sender_hub);

            m_gather_network_router_model->updateDynamicEnergyCrossbar(m_gather_network_link_width/2, \
                  computeProcessingTime(pkt_length, m_gather_network_link_width) * m_num_hops_sender_core_to_sender_hub);
            m_gather_network_router_model->updateDynamicEnergyClock( \
                  computeProcessingTime(pkt_length, m_gather_network_link_width) * m_num_hops_sender_core_to_sender_hub);
            // We assume that there is no buffering here - so dont update dynamic energy of buffer

            // Update Dynamic Energy of link
            m_gather_network_link_model->updateDynamicEnergy(m_gather_network_link_width / 2, \
                  computeProcessingTime(pkt_length, m_gather_network_link_width) * m_num_hops_sender_core_to_sender_hub);
         }

         // Activity Counters
         // Router
         m_gather_network_router_switch_allocator_traversals += m_num_hops_sender_core_to_sender_hub;
         m_gather_network_router_crossbar_traversals += (computeProcessingTime(pkt_length, m_gather_network_link_width) * \
                                                        m_num_hops_sender_core_to_sender_hub);
         
         // Link      
         m_gather_network_link_traversals += (computeProcessingTime(pkt_length, m_gather_network_link_width) * \
                                             m_num_hops_sender_core_to_sender_hub);
         break;

      case OPTICAL_NETWORK:
         if (Config::getSingleton()->getEnablePowerModeling())
         {
            m_optical_network_link_model->updateDynamicEnergy(m_optical_network_link_width / 2, \
                  computeProcessingTime(pkt_length, m_optical_network_link_width));
         }
         m_optical_network_link_traversals += computeProcessingTime(pkt_length, m_optical_network_link_width);
         break;

      case SCATTER_NETWORK:
         // We look at a tree and compute the number of hops in the tree
         if (Config::getSingleton()->getEnablePowerModeling())
         {
            m_scatter_network_link_model->updateDynamicEnergy(m_scatter_network_link_width / 2, \
                  computeProcessingTime(pkt_length, m_scatter_network_link_width) * m_cluster_size);
         }
         m_scatter_network_link_traversals += computeProcessingTime(pkt_length, m_scatter_network_link_width) * \
                                              m_cluster_size;
         break;

      default:
         LOG_PRINT_ERROR("Unrecognized Sub Network Type(%u)", sub_net_type);
         break;
   }
}

void
NetworkModelAtacCluster::outputPowerSummary(ostream& out)
{
   if (Config::getSingleton()->getEnablePowerModeling())
   {
      volatile double total_static_power = 0.0;
      if (m_core_id == getCoreIDWithOpticalHub(getClusterID(m_core_id)))
      {
         // I have an optical Hub
         // m_cluster_size is the number of cores in the cluster + 1 Hub
         // So, a total of (m_cluster_size + 1) nodes
         // We need m_cluster_size edges to connect them to form a tree

         // Get the static power for the entire network.
         // This function will only be called only on Core0

         // Gather network is modeled as an electrical mesh
         volatile double static_power_gather_network = m_cluster_size * (m_gather_network_router_model->getTotalStaticPower() + \
               m_gather_network_link_model->getStaticPower() * (m_num_gather_network_router_ports-1));

         volatile double static_power_optical_network = m_optical_network_link_model->getStaticPower();

         // Scatter network is modeled as an H-tree
         UInt32 total_scatter_network_links = m_cluster_size;
         volatile double static_power_scatter_network = m_scatter_network_link_model->getStaticPower() * total_scatter_network_links;

         total_static_power = (static_power_gather_network + static_power_optical_network + static_power_scatter_network * m_num_scatter_networks_per_cluster);
      }

      // This function is called on all the cores
      volatile double total_dynamic_energy = m_gather_network_router_model->getTotalDynamicEnergy() + \
                                             m_gather_network_link_model->getDynamicEnergy() + \
                                             m_optical_network_link_model->getDynamicEnergy() + \
                                             m_scatter_network_link_model->getDynamicEnergy();

      out << "    Static Power: " << total_static_power << endl;
      out << "    Dynamic Energy: " << total_dynamic_energy << endl;
   }

   out << "  Activity Counters:" << endl;
   out << "    Gather Network Router Switch Allocator Traversals: " << m_gather_network_router_switch_allocator_traversals << endl;
   out << "    Gather Network Router Crossbar Traversals: " << m_gather_network_router_crossbar_traversals << endl;
   out << "    Gather Network Link Traversals: " << m_gather_network_link_traversals << endl;
   out << "    Optical Network Link Traversals: " << m_optical_network_link_traversals << endl;
   out << "    Scatter Network Link Traversals: " << m_scatter_network_link_traversals << endl;
}

#ifdef TRACK_UTILIZATION
void
NetworkModelAtacCluster::initializeUtilizationCounters()
{
   m_update_interval = 10000;
   m_receiver_hub_utilization = new vector<UInt64>[m_num_scatter_networks_per_cluster];
}

void
NetworkModelAtacCluster::destroyUtilizationCounters()
{
   delete [] m_receiver_hub_utilization;
}

void
NetworkModelAtacCluster::updateUtilization(NetworkComponentType hub_type, SInt32 hub_id, UInt64 pkt_time, UInt64 processing_time)
{
   UInt64 interval_id = pkt_time / m_update_interval;
   switch (hub_type)
   {
   case SENDER_HUB:
      updateVector(interval_id, m_sender_hub_utilization);
      m_sender_hub_utilization[interval_id] += processing_time;
      break;

   case RECEIVER_HUB:
      updateVector(interval_id, m_receiver_hub_utilization[hub_id]);
      m_receiver_hub_utilization[hub_id][interval_id] += processing_time;
      break;

   default:
      LOG_PRINT_ERROR("Unrecognized hub_type(%u)", hub_type);
      break;
   }
}

void
NetworkModelAtacCluster::updateVector(UInt64 interval_id, vector<UInt64>& utilization_vec)
{
   if (interval_id >= utilization_vec.size())
   {
      UInt64 next_interval = utilization_vec.size();
      for (; next_interval <= interval_id; next_interval++)
         utilization_vec.push_back(0);
   }
   assert(interval_id < utilization_vec.size());
}

void
NetworkModelAtacCluster::outputUtilizationSummary()
{
   stringstream filename;
   string output_dir = Sim()->getCfg()->getString("general/output_dir", "./output_files/"); 
   filename << output_dir << "utilization_" << getNetwork()->getCore()->getId(); 
   ofstream utilization_file((filename.str()).c_str());

   assert(m_num_scatter_networks_per_cluster == 2);
   UInt32 size = getMin<UInt32>(m_sender_hub_utilization.size(), \
        m_receiver_hub_utilization[0].size(), m_receiver_hub_utilization[1].size()); 
   for (UInt32 i = 0; i < size; i++)
   {
      float utilization = ( (float) (m_sender_hub_utilization[i] + m_receiver_hub_utilization[0][i] + \
               m_receiver_hub_utilization[1][i]) ) / m_update_interval;
      utilization_file << i << "\t" << utilization / 3 << endl;
   }
   utilization_file.close();
}
#endif