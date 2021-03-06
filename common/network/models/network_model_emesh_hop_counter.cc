#include <stdlib.h>
#include <math.h>

#include "network_model_emesh_hop_counter.h"
#include "simulator.h"
#include "config.h"
#include "config.h"
#include "core.h"
#include "clock_converter.h"

NetworkModelEMeshHopCounter::NetworkModelEMeshHopCounter(Network *net, SInt32 network_id)
   : NetworkModel(net, network_id)
{
   _flit_width = Sim()->getCfg()->getInt("network/emesh/flit_width");

   SInt32 total_cores = Config::getSingleton()->getTotalCores();
   _mesh_width = (SInt32) floor (sqrt(total_cores));
   _mesh_height = (SInt32) ceil (1.0 * total_cores / _mesh_width);

   try
   {
      _frequency = Sim()->getCfg()->getFloat("network/emesh/frequency");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read emesh_hop_counter paramters from the cfg file");
   }

   assert(total_cores <= _mesh_width * _mesh_height);
   assert(total_cores > (_mesh_width - 1) * _mesh_height);
   assert(total_cores > _mesh_width * (_mesh_height - 1));

   // Create Rounter & Link Models
   createRouterAndLinkModels();

   // Create Sender and Receiver Contention Models
   _sender_contention_model = new QueueModelSimple();
   _receiver_contention_model = new QueueModelSimple();
}

NetworkModelEMeshHopCounter::~NetworkModelEMeshHopCounter()
{
   // Destroy Sender & Receiver Contention Models
   delete _sender_contention_model;
   delete _receiver_contention_model;

   // Destroy the Router & Link Models
   destroyRouterAndLinkModels();
}

void
NetworkModelEMeshHopCounter::createRouterAndLinkModels()
{
   UInt64 router_delay = 0;
   UInt32 num_flits_per_output_buffer = 1;   // Here, contention is not modeled
   double link_length = _tile_width;
   string link_type;

   try
   {
      link_type = Sim()->getCfg()->getString("network/emesh/link_type");

      // Pipeline delay of the router in clock cycles
      router_delay = (UInt64) Sim()->getCfg()->getInt("network/emesh/router/data_pipeline_delay");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read emesh_hop_counter link and router parameters");
   }

   // Create Router & Link Models
   // Right now,
   // Router model yields only power
   // Link model yields delay & power
   // They both will be later augmented to yield area

   // Assume,
   // Router & Link have the same throughput (flit_width = phit_width = link_width)
   // Router & Link are clocked at the same frequency
   
   _num_router_ports = 5;

   _electrical_router_power_model = RouterPowerModel::create(_num_router_ports,
         _num_router_ports, num_flits_per_output_buffer, _flit_width);
   
   _electrical_link_performance_model = ElectricalLinkPerformanceModel::create(link_type,
         _frequency, link_length, _flit_width, 1 /* fanout */);
   _electrical_link_power_model = ElectricalLinkPowerModel::create(link_type,
         _frequency, link_length, _flit_width, 1 /* fanout */);

   // It is possible that one hop can be accomodated in one cycles by
   // intelligent circuit design but for simplicity, here we consider
   // that a link takes 1 cycle

   // NetworkLinkModel::getDelay() gets delay in cycles (clock frequency is the link frequency)
   // The link frequency is the same as the network frequency here
   UInt64 link_delay = _electrical_link_performance_model->getDelay();
   LOG_ASSERT_WARNING(link_delay <= 1, "Network Link Delay(%llu) exceeds 1 cycle", link_delay);
   
   _hop_latency = router_delay + link_delay;

   // Initialize Event Counters
   initializeEventCounters();
}

void
NetworkModelEMeshHopCounter::initializeEventCounters()
{
   _total_switch_allocator_requests = 0;
   _total_crossbar_traversals = 0;
   _total_link_traversals = 0;
}

void
NetworkModelEMeshHopCounter::destroyRouterAndLinkModels()
{
   delete _electrical_router_power_model;
   delete _electrical_link_performance_model;
   delete _electrical_link_power_model;
}

void
NetworkModelEMeshHopCounter::computePosition(core_id_t core,
                                             SInt32 &x, SInt32 &y)
{
   x = core % _mesh_width;
   y = core / _mesh_width;
}

SInt32
NetworkModelEMeshHopCounter::computeDistance(core_id_t sender, core_id_t receiver)
{
   SInt32 sx, sy, dx, dy;
   computePosition(sender, sx, sy);
   computePosition(receiver, dx, dy);
   return abs(sx - dx) + abs(sy - dy);
}

UInt64
NetworkModelEMeshHopCounter::computeLatency(core_id_t sender, core_id_t receiver)
{
   return (computeDistance(sender, receiver) * _hop_latency);
}

UInt32
NetworkModelEMeshHopCounter::computeAction(const NetPacket& pkt)
{
   if (pkt.specific == RECEIVER_CORE)
      return RoutingAction::RECEIVE;
   else if ((pkt.specific == SENDER_ROUTER) || (pkt.specific == RECEIVER_ROUTER))
      return RoutingAction::FORWARD;
   else
   {
      LOG_PRINT_ERROR("pkt.specific(%u)", pkt.specific);
      return RoutingAction::DROP;
   }
}

void
NetworkModelEMeshHopCounter::routePacket(const NetPacket &pkt,
                                         std::vector<Hop> &next_hops)
{
   assert(_enabled);

   UInt64 serialization_latency = computeSerializationLatency(&pkt);

   if (pkt.specific == SENDER_CORE)
   {
      assert(pkt.sender == _core_id);

      Hop h;
      h.final_dest = pkt.receiver;
      h.next_dest = _core_id;
      h.time = pkt.time;
      h.specific = SENDER_ROUTER;

      next_hops.push_back(h);

   } // (pkt.specific == SENDER_CORE)

   else if (pkt.specific == SENDER_ROUTER)
   {
      assert(pkt.sender == _core_id);

      if (pkt.receiver == NetPacket::BROADCAST)
      {
         // Contention Delay at the sender - Assume some sort of broadcast delay here
         UInt64 sender_contention_delay = _sender_contention_model->computeQueueDelay(pkt.time, serialization_latency);
         
         for (SInt32 i = 0; i < (SInt32) Config::getSingleton()->getTotalCores(); i++)
         {
            UInt64 latency = computeLatency(pkt.sender, i);
            UInt32 next_module = RECEIVER_CORE;

            if (i != pkt.sender)
            {
               SInt32 num_hops = computeDistance(pkt.sender, i);
               // Update the Dynamic Energy - Need to update the dynamic energy for all routers to the destination
               // We dont keep track of contention here. So, assume contention = 0
               updateDynamicEnergy(pkt, _num_router_ports/2, num_hops);
    
               latency += sender_contention_delay;
               next_module = RECEIVER_ROUTER;
            }

            Hop h;
            h.final_dest = NetPacket::BROADCAST;
            h.next_dest = i;
            h.time = pkt.time + latency;
            h.specific = next_module;

            next_hops.push_back(h);
         }
      }
      else
      {
         UInt64 latency = computeLatency(pkt.sender, pkt.receiver);
         UInt32 next_module = RECEIVER_CORE;

         if (pkt.receiver != pkt.sender)
         {
            SInt32 num_hops = computeDistance(pkt.sender, pkt.receiver);
            // Update the Dynamic Energy - Need to update the dynamic energy for all routers to the destination
            // We dont keep track of contention here. So, assume contention = 0
            updateDynamicEnergy(pkt, _num_router_ports/2, num_hops);
            
            // Contention Delay at the sender
            UInt64 sender_contention_delay = _sender_contention_model->computeQueueDelay(pkt.time, serialization_latency);

            latency += sender_contention_delay;
            next_module = RECEIVER_ROUTER;
         }

         Hop h;
         h.final_dest = pkt.receiver;
         h.next_dest = pkt.receiver;
         h.time = pkt.time + latency;
         h.specific = next_module;

         next_hops.push_back(h);
      }
   } // (pkt.specific == SENDER_ROUTER)

   else if (pkt.specific == RECEIVER_ROUTER)
   {
      assert(pkt.sender != getNetwork()->getCore()->getId());
      
      // Receiver Contention Models
      UInt64 receiver_contention_delay = _receiver_contention_model->computeQueueDelay(pkt.time, serialization_latency);

      Hop h;
      h.final_dest = pkt.receiver;
      h.next_dest = getNetwork()->getCore()->getId();
      // Add receiver_contention_delay and serialization_latency
      h.time = pkt.time + receiver_contention_delay + serialization_latency;
      h.specific = RECEIVER_CORE;

      next_hops.push_back(h);
   } // (pkt.specific == RECEIVER_ROUTER)

   else
   {
      LOG_PRINT_ERROR("Cannot Reach Here, pkt.specific(%u)", pkt.specific);
   }
}

void
NetworkModelEMeshHopCounter::processReceivedPacket(const NetPacket* packet)
{
   assert(_enabled);
   assert(packet->specific == RECEIVER_CORE);

   UInt64 zero_load_latency = computeLatency(packet->sender, _core_id);
   updatePacketReceiveStatistics(packet, zero_load_latency);
}

void
NetworkModelEMeshHopCounter::outputSummary(std::ostream &out)
{
   NetworkModel::outputSummary(out);
   outputPowerSummary(out);
}

// Power/Energy related functions
void
NetworkModelEMeshHopCounter::updateDynamicEnergy(const NetPacket& pkt,
      UInt32 contention, UInt32 num_hops)
{
   // For now, assume that half of the bits in the packet flip
   UInt32 num_flits = computeSerializationLatency(&pkt);
      
   // Dynamic Energy Dissipated

   // 1) Electrical Router
   // For every activity, update the dynamic energy due to the clock

   // Assume half of the input ports are contending for the same output port
   // Switch allocation is only done for the head flit. All the other flits just follow.
   // So, we dont need to update dynamic energies again
   if (Config::getSingleton()->getEnablePowerModeling())
   {
      _electrical_router_power_model->updateDynamicEnergySwitchAllocator(contention, num_hops);
      _electrical_router_power_model->updateDynamicEnergyClock(num_hops);
   }
   _total_switch_allocator_requests += num_hops;

   // Assume half of the bits flip while crossing the crossbar 
   if (Config::getSingleton()->getEnablePowerModeling())
   {
      _electrical_router_power_model->updateDynamicEnergyCrossbar(_flit_width/2, num_flits * num_hops); 
      _electrical_router_power_model->updateDynamicEnergyClock(num_flits * num_hops);
   }
   _total_crossbar_traversals += (num_flits * num_hops);
  
   // 2) Electrical Link
   if (Config::getSingleton()->getEnablePowerModeling())
   {
      _electrical_link_power_model->updateDynamicEnergy(_flit_width/2, num_flits * num_hops);
   }
   _total_link_traversals += (num_flits * num_hops);
}

void
NetworkModelEMeshHopCounter::outputPowerSummary(ostream& out)
{
   if (Config::getSingleton()->getEnablePowerModeling())
   {
      // We need to get the power of the router + all the outgoing links (a total of 5 links)
      volatile double static_power = _electrical_router_power_model->getTotalStaticPower() +
                                     (_electrical_link_power_model->getStaticPower() * _num_router_ports);
      volatile double dynamic_energy = _electrical_router_power_model->getTotalDynamicEnergy() +
                                       _electrical_link_power_model->getDynamicEnergy();

      out << "    Static Power (in watts): " << static_power << endl;
      out << "    Dynamic Energy (in joules): " << dynamic_energy << endl;
   }

   out << "   Event Counters:" << endl;
   out << "    Switch Allocator Requests: " << _total_switch_allocator_requests << endl;
   out << "    Crossbar Traversals: " << _total_crossbar_traversals << endl;
   out << "    Link Traversals: " << _total_link_traversals << endl;
}
