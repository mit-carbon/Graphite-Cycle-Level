#pragma once

#include "router_power_model.h"
#include "contrib/orion/orion.h"

class RouterPowerModelOrion : public RouterPowerModel
{
public:
   RouterPowerModelOrion(UInt32 num_input_ports, UInt32 num_output_ports, \
         UInt32 input_buffer_size, UInt32 flit_width);
   ~RouterPowerModelOrion();

   // Update Dynamic Energy
   void updateDynamicEnergyBuffer(BufferAccess::type_t buffer_access_type, UInt32 num_bit_flips, UInt32 num_flits = 1)
   {
      bool is_read = (buffer_access_type == BufferAccess::READ) ? true : false;
      volatile double dynamic_energy_buffer = _orion_router->calc_dynamic_energy_buf(is_read);
      _total_dynamic_energy_buffer += (num_flits * dynamic_energy_buffer);
   }
   void updateDynamicEnergyCrossbar(UInt32 num_bit_flips, UInt32 num_flits = 1)
   {
      volatile double dynamic_energy_crossbar = _orion_router->calc_dynamic_energy_xbar();
      _total_dynamic_energy_crossbar += (num_flits * dynamic_energy_crossbar);
   }
   void updateDynamicEnergySwitchAllocator(UInt32 num_requests, UInt32 num_flits = 1)
   {
      volatile double dynamic_energy_switch_allocator = _orion_router->calc_dynamic_energy_global_sw_arb(num_requests);
      _total_dynamic_energy_switch_allocator += (num_flits * dynamic_energy_switch_allocator);
   }
   void updateDynamicEnergyClock(UInt32 num_flits = 1)
   {
      volatile double dynamic_energy_clock = _orion_router->calc_dynamic_energy_clock();
      _total_dynamic_energy_clock += (num_flits * dynamic_energy_clock);
   }
   void updateDynamicEnergy(UInt32 num_bit_flips, UInt32 num_flits = 1)
   {
      updateDynamicEnergyBuffer(BufferAccess::WRITE, num_bit_flips, num_flits);
      updateDynamicEnergyBuffer(BufferAccess::READ, num_bit_flips, num_flits);
      updateDynamicEnergySwitchAllocator(_num_input_ports/2);
      updateDynamicEnergyCrossbar(num_bit_flips, num_flits);
      updateDynamicEnergyClock(3 * num_flits + 1);
   }

   // Get Dynamic Energy
   volatile double getDynamicEnergyBuffer() { return _total_dynamic_energy_buffer; }
   volatile double getDynamicEnergyCrossbar() { return _total_dynamic_energy_crossbar; }
   volatile double getDynamicEnergySwitchAllocator() { return _total_dynamic_energy_switch_allocator; }
   volatile double getDynamicEnergyClock() { return _total_dynamic_energy_clock; }
   volatile double getTotalDynamicEnergy()
   {
      return (_total_dynamic_energy_buffer + _total_dynamic_energy_crossbar + _total_dynamic_energy_switch_allocator + _total_dynamic_energy_clock);
   }
   
   // Static Power
   volatile double getStaticPowerBuffer()
   {
      return _orion_router->get_static_power_buf();
   }
   volatile double getStaticPowerBufferCrossbar()
   {
      return _orion_router->get_static_power_xbar();
   }
   volatile double getStaticPowerSwitchAllocator()
   {
      return _orion_router->get_static_power_sa();
   }
   volatile double getStaticPowerClock()
   {
      return _orion_router->get_static_power_clock();
   }
   volatile double getTotalStaticPower()
   {
      return (_orion_router->get_static_power_buf() + _orion_router->get_static_power_xbar() + _orion_router->get_static_power_sa() + _orion_router->get_static_power_clock());
   }

   // Reset Counters
   void resetCounters()
   {
      initializeCounters();
   }

private:
   OrionRouter* _orion_router;

   volatile double _total_dynamic_energy_buffer;
   volatile double _total_dynamic_energy_crossbar;
   volatile double _total_dynamic_energy_switch_allocator;
   volatile double _total_dynamic_energy_clock;

   // Private Functions
   void initializeCounters();
};
