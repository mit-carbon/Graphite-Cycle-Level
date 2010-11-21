#pragma once

#include "electrical_link_power_model.h"
#include "fixed_types.h"

class ElectricalLinkPowerModelEqualized : public ElectricalLinkPowerModel
{
public:
   ElectricalLinkPowerModelEqualized(volatile float link_frequency, \
         volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints);
   ~ElectricalLinkPowerModelEqualized();
   
   volatile double getStaticPower();
   void updateDynamicEnergy(UInt32 num_bit_flips, UInt32 num_flits = 1);
   volatile double getDynamicEnergy() { return _total_dynamic_energy; }

   void resetCounters() { _total_dynamic_energy = 0.0; }

private:
   // Static Power Parameters
   volatile double _static_link_power_per_mm;
   volatile double _fixed_power;

   // Dynamic Power Parameters
   volatile double _dynamic_tx_energy_per_mm;
   volatile double _dynamic_rx_energy_per_mm;
   volatile double _total_dynamic_energy;
};
