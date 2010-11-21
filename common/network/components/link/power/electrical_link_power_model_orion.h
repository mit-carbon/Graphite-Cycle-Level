#pragma once

#include "electrical_link_power_model.h"
#include "contrib/orion/orion.h"

class ElectricalLinkPowerModelOrion : public ElectricalLinkPowerModel
{
public:
   ElectricalLinkPowerModelOrion(ElectricalLinkModel::Type link_type, volatile float link_frequency, \
         volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints);
   ~ElectricalLinkPowerModelOrion();

   volatile double getStaticPower();
   void updateDynamicEnergy(UInt32 num_bit_flips, UInt32 num_flits = 1);
   volatile double getDynamicEnergy() { return _total_dynamic_energy; }

   void resetCounters() { _total_dynamic_energy = 0; }

private:
   OrionLink* _orion_link;
   volatile double _total_dynamic_energy;
};
