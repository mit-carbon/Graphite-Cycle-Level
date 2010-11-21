#pragma once

#include "electrical_link_power_model.h"
#include "fixed_types.h"

class ElectricalLinkPowerModelRepeated : public ElectricalLinkPowerModel
{
public:
   ElectricalLinkPowerModelRepeated(volatile float link_frequency, \
         volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints);
   ~ElectricalLinkPowerModelRepeated();

   volatile double getStaticPower();
   void updateDynamicEnergy(UInt32 num_bit_flips, UInt32 num_flits = 1);
   volatile double getDynamicEnergy() { return _total_dynamic_energy; }

   void resetCounters() { _total_dynamic_energy = 0; }

private:
   // Static Power Parameters
   volatile double _static_link_power_per_mm;

   // Dynamic Power Parameters
   volatile double _dynamic_link_energy_per_mm;
   volatile double _total_dynamic_energy;
};
