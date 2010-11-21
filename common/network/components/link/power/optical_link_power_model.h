#pragma once

#include "optical_link_model.h"
#include "link_power_model.h"
#include "fixed_types.h"

class OpticalLinkPowerModel : public OpticalLinkModel, public LinkPowerModel
{
public:
   OpticalLinkPowerModel(volatile float link_frequency, volatile double link_length, \
         UInt32 link_width, SInt32 num_receiver_endpoints);
   ~OpticalLinkPowerModel();

   volatile double getStaticPower();
   void updateDynamicEnergy(UInt32 num_bit_flips, UInt32 num_flits = 1);
   volatile double getDynamicEnergy() { return _total_dynamic_energy_sender + _total_dynamic_energy_receiver; }

   // Public functions special to OpticalLink
   volatile double getLaserPower();
   volatile double getRingTuningPower();
   volatile double getDynamicEnergySender();
   volatile double getDynamicEnergyReceiver();

   void resetCounters() { _total_dynamic_energy_sender = _total_dynamic_energy_receiver = 0; }

private:
   // Static Power parameters
   volatile double _ring_tuning_power;
   volatile double _laser_power;
   volatile double _electrical_tx_static_power;
   volatile double _electrical_rx_static_power;
   volatile double _clock_static_power_tx;
   volatile double _clock_static_power_rx;

   // Dynamic Power parameters
   volatile double _electrical_tx_dynamic_energy;
   volatile double _electrical_rx_dynamic_energy;
   volatile double _total_dynamic_energy_sender;
   volatile double _total_dynamic_energy_receiver;
};
