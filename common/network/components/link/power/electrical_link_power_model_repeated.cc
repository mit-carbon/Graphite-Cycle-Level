#include <cmath>

#include "electrical_link_power_model_repeated.h"
#include "simulator.h"
#include "config.h"
#include "log.h"

ElectricalLinkPowerModelRepeated::ElectricalLinkPowerModelRepeated(volatile float link_frequency, \
      volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints):
   ElectricalLinkPowerModel(link_frequency, link_length, link_width, num_receiver_endpoints),
   _total_dynamic_energy(0)
{
   try
   {
      // Static Power Parameters
      _static_link_power_per_mm = Sim()->getCfg()->getFloat( \
            "link_model/electrical_repeated/power/static/static_power_per_mm");

      // Dynamic Power Parameters
      _dynamic_link_energy_per_mm = Sim()->getCfg()->getFloat( \
            "link_model/electrical_repeated/power/dynamic/dynamic_energy_per_mm");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Unable to read repeated electrical link parameters from the cfg file");
   }
}

ElectricalLinkPowerModelRepeated::~ElectricalLinkPowerModelRepeated()
{}

volatile double
ElectricalLinkPowerModelRepeated::getStaticPower()
{
   return _link_width * _static_link_power_per_mm * _link_length;
}

void
ElectricalLinkPowerModelRepeated::updateDynamicEnergy(UInt32 num_bit_flips, UInt32 num_flits)
{
   _total_dynamic_energy += (num_flits * (num_bit_flips * _dynamic_link_energy_per_mm * _link_length));
}
