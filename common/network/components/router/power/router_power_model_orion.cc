#include "router_power_model_orion.h"

RouterPowerModelOrion::RouterPowerModelOrion(UInt32 num_input_ports, UInt32 num_output_ports, \
      UInt32 input_buffer_size, UInt32 flit_width):
   RouterPowerModel(num_input_ports, num_output_ports, input_buffer_size, flit_width)
{
   _orion_router = new OrionRouter(num_input_ports, num_output_ports, 1, 1, \
         input_buffer_size, flit_width, OrionConfig::getSingleton());
   initializeCounters();
}

RouterPowerModelOrion::~RouterPowerModelOrion()
{
   delete _orion_router;
}

void
RouterPowerModelOrion::initializeCounters()
{
   _total_dynamic_energy_buffer = 0;
   _total_dynamic_energy_crossbar = 0;
   _total_dynamic_energy_switch_allocator = 0;
   _total_dynamic_energy_clock = 0;
}
