#include <stdlib.h>
#include <math.h>

#include "network_model_emesh_hop_counter.h"
#include "simulator.h"
#include "config.h"
#include "config.h"
#include "core.h"
#include "memory_manager_base.h"
#include "clock_converter.h"

UInt32 NetworkModelEMeshHopCounter::_NUM_OUTPUT_DIRECTIONS = 4;

NetworkModelEMeshHopCounter::NetworkModelEMeshHopCounter(Network *net, SInt32 network_id)
   : NetworkModel(net, network_id)
   , _enabled(false)
{
   SInt32 total_cores = Config::getSingleton()->getTotalCores();

   _mesh_width = (SInt32) floor (sqrt(total_cores));
   _mesh_height = (SInt32) ceil (1.0 * total_cores / _mesh_width);

   try
   {
      _frequency = Sim()->getCfg()->getFloat("network/emesh_hop_counter/frequency");
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

   // Initialize Performance Counters
   initializePerformanceCounters();
}

NetworkModelEMeshHopCounter::~NetworkModelEMeshHopCounter()
{
   // Destroy the Router & Link Models
   destroyRouterAndLinkModels();
}

void
NetworkModelEMeshHopCounter::createRouterAndLinkModels()
{
   UInt64 router_delay = 0;
   UInt32 num_flits_per_output_buffer = 1;   // Here, contention is not modeled
   volatile double link_length;
   try
   {
      _link_type = Sim()->getCfg()->getString("network/emesh_hop_counter/link/type");
      _link_width = Sim()->getCfg()->getInt("network/emesh_hop_counter/link/width");
      link_length = Sim()->getCfg()->getFloat("network/emesh_hop_counter/link/length");

      // Pipeline delay of the router in clock cycles
      router_delay = (UInt64) Sim()->getCfg()->getInt("network/emesh_hop_counter/router/delay");
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

   _electrical_router_power_model = RouterPowerModel::create(_num_router_ports, \
         _num_router_ports, num_flits_per_output_buffer, _link_width);
   
   _electrical_link_performance_model = ElectricalLinkPerformanceModel::create(_link_type, \
         _frequency, link_length, _link_width, 1 /* fanout */);
   _electrical_link_power_model = ElectricalLinkPowerModel::create(_link_type, \
         _frequency, link_length, _link_width, 1 /* fanout */);

   // It is possible that one hop can be accomodated in one cycles by
   // intelligent circuit design but for simplicity, here we consider
   // that a link takes 1 cycle

   // NetworkLinkModel::getDelay() gets delay in cycles (clock frequency is the link frequency)
   // The link frequency is the same as the network frequency here
   UInt64 link_delay = _electrical_link_performance_model->getDelay();
   LOG_ASSERT_WARNING(link_delay <= 1, "Network Link Delay(%llu) exceeds 1 cycle", link_delay);
   
   _hop_latency = router_delay + link_delay;

   initializeActivityCounters();
}

void
NetworkModelEMeshHopCounter::initializeActivityCounters()
{
   _switch_allocator_traversals = 0;
   _crossbar_traversals = 0;
   _link_traversals = 0;
}

void
NetworkModelEMeshHopCounter::destroyRouterAndLinkModels()
{
   delete _electrical_router_power_model;
   delete _electrical_link_performance_model;
   delete _electrical_link_power_model;
}

void
NetworkModelEMeshHopCounter::initializePerformanceCounters()
{
   _num_packets = 0;
   _num_bytes = 0;
   _total_latency = 0;
}

void
NetworkModelEMeshHopCounter::computePosition(core_id_t core,
                                             SInt32 &x, SInt32 &y)
{
   x = core % _mesh_width;
   y = core / _mesh_width;
}

SInt32
NetworkModelEMeshHopCounter::computeDistance(SInt32 x1, SInt32 y1, SInt32 x2, SInt32 y2)
{
   return abs(x1 - x2) + abs(y1 - y2);
}

UInt32
NetworkModelEMeshHopCounter::computeAction(const NetPacket& pkt)
{
   core_id_t core_id = getNetwork()->getCore()->getId();
   LOG_ASSERT_ERROR((pkt.receiver == NetPacket::BROADCAST) || (pkt.receiver == core_id), \
         "pkt.receiver(%i), core_id(%i)", pkt.receiver, core_id);

   return RoutingAction::RECEIVE;
}

void
NetworkModelEMeshHopCounter::routePacket(const NetPacket &pkt,
                                         std::vector<Hop> &next_hops)
{
   UInt32 pkt_length = getNetwork()->getModeledLength(pkt);

   UInt64 serialization_latency = computeProcessingTime(pkt_length);

   SInt32 sx, sy, dx, dy;

   computePosition(pkt.sender, sx, sy);

   if (pkt.receiver == NetPacket::BROADCAST)
   {
      UInt32 total_cores = Config::getSingleton()->getTotalCores();
   
      UInt64 curr_time = pkt.time;
      // There's no broadcast tree here, but I guess that won't be a
      // bottleneck at all since there's no contention
      for (SInt32 i = 0; i < (SInt32) total_cores; i++)
      {
         computePosition(i, dx, dy);

         UInt32 num_hops = computeDistance(sx, sy, dx, dy);
         UInt64 latency = num_hops * _hop_latency;

         if (i != pkt.sender)
         {
            // Update the Dynamic Energy - Need to update the dynamic energy for all routers to the destination
            // We dont keep track of contention here. So, assume contention = 0
            updateDynamicEnergy(pkt, _num_router_ports/2, num_hops);
            
            latency += serialization_latency;
         }

         Hop h;
         h.final_dest = NetPacket::BROADCAST;
         h.next_dest = i;
         h.time = curr_time + latency;

         // Update curr_time
         curr_time += serialization_latency;

         next_hops.push_back(h);
      }
   }
   else
   {
      computePosition(pkt.receiver, dx, dy);

      UInt32 num_hops = computeDistance(sx, sy, dx, dy);
      UInt64 latency = num_hops * _hop_latency;

      if (pkt.receiver != pkt.sender)
      {
         // Update the Dynamic Energy - Need to update the dynamic energy for all routers to the destination
         // We dont keep track of contention here. So, assume contention = 0
         updateDynamicEnergy(pkt, _num_router_ports/2, num_hops);
         
         latency += serialization_latency;
      }

      Hop h;
      h.final_dest = pkt.receiver;
      h.next_dest = pkt.receiver;
      h.time = pkt.time + latency;

      next_hops.push_back(h);
   }
}

void
NetworkModelEMeshHopCounter::processReceivedPacket(NetPacket &pkt)
{
   ScopedLock sl(_lock);

   core_id_t requester = getRequester(pkt);
   if ((!_enabled) || (requester >= (core_id_t) Config::getSingleton()->getApplicationCores()))
      return;

   UInt32 pkt_length = getNetwork()->getModeledLength(pkt);

   // LOG_ASSERT_ERROR(pkt.start_time > 0, "start_time(%llu)", pkt.start_time);

   UInt64 latency = pkt.time - pkt.start_time;

   _num_packets ++;
   _num_bytes += pkt_length;
   _total_latency += latency;
}

UInt64 
NetworkModelEMeshHopCounter::computeProcessingTime(UInt32 pkt_length)
{
   // Send: (pkt_length * 8) bits
   // Bandwidth: (_link_width) bits/cycle
   UInt32 num_bits = pkt_length * 8;
   if (num_bits % _link_width == 0)
      return (UInt64) (num_bits/_link_width);
   else
      return (UInt64) (num_bits/_link_width + 1);
}

core_id_t
NetworkModelEMeshHopCounter::getRequester(const NetPacket& pkt)
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

void
NetworkModelEMeshHopCounter::reset()
{
   // Performance Counters
   initializePerformanceCounters();
   
   // Activity Counters
   initializeActivityCounters();
   
   // Router & Link Models
   _electrical_router_power_model->resetCounters();
   _electrical_link_power_model->resetCounters();
}

void
NetworkModelEMeshHopCounter::outputSummary(std::ostream &out)
{
   out << "    num packets received: " << _num_packets << std::endl;
   out << "    num bytes received: " << _num_bytes << std::endl;
   UInt64 total_latency_in_ns = convertCycleCount(_total_latency, _frequency, 1.0);
   out << "    average latency (in clock cycles): " << ((float) _total_latency) / _num_packets << std::endl;
   out << "    average latency (in ns): " << ((float) total_latency_in_ns) / _num_packets << std::endl;

   outputPowerSummary(out);
}

// Power/Energy related functions
void
NetworkModelEMeshHopCounter::updateDynamicEnergy(const NetPacket& pkt,
      UInt32 contention, UInt32 num_hops)
{
   core_id_t requester = getRequester(pkt);
   if ((!_enabled) || (requester >= (core_id_t) Config::getSingleton()->getApplicationCores()))
      return;

   // TODO: Make these models detailed later - Compute exact number of bit flips
   UInt32 pkt_length = getNetwork()->getModeledLength(pkt);
   // For now, assume that half of the bits in the packet flip
   UInt32 num_flits = computeProcessingTime(pkt_length);
      
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
   _switch_allocator_traversals += num_hops;

   // Assume half of the bits flip while crossing the crossbar 
   if (Config::getSingleton()->getEnablePowerModeling())
   {
      _electrical_router_power_model->updateDynamicEnergyCrossbar(_link_width/2, num_flits * num_hops); 
      _electrical_router_power_model->updateDynamicEnergyClock(num_flits * num_hops);
   }
   _crossbar_traversals += (num_flits * num_hops);
  
   // 2) Electrical Link
   if (Config::getSingleton()->getEnablePowerModeling())
   {
      _electrical_link_power_model->updateDynamicEnergy(_link_width/2, num_flits * num_hops);
   }
   _link_traversals += (num_flits * num_hops);
}

void
NetworkModelEMeshHopCounter::outputPowerSummary(ostream& out)
{
   if (Config::getSingleton()->getEnablePowerModeling())
   {
      LOG_PRINT("Router Static Power(%g), Link Static Power(%g)", \
            _electrical_router_power_model->getTotalStaticPower(), \
            _electrical_link_power_model->getStaticPower() * _NUM_OUTPUT_DIRECTIONS);

      // We need to get the power of the router + all the outgoing links (a total of 4 outputs)
      volatile double static_power = _electrical_router_power_model->getTotalStaticPower() + \
                                     (_electrical_link_power_model->getStaticPower() * _NUM_OUTPUT_DIRECTIONS);
      volatile double dynamic_energy = _electrical_router_power_model->getTotalDynamicEnergy() + \
                                       _electrical_link_power_model->getDynamicEnergy();

      out << "    Static Power: " << static_power << endl;
      out << "    Dynamic Energy: " << dynamic_energy << endl;
   }

   out << "  Activity Counters:" << endl;
   out << "    Switch Allocator Traversals: " << _switch_allocator_traversals << endl;
   out << "    Crossbar Traversals: " << _crossbar_traversals << endl;
   out << "    Link Traversals: " << _link_traversals << endl;
}
