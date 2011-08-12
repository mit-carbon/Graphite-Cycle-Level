#include <cmath>

#include "optical_link_power_model.h"
#include "simulator.h"
#include "config.h"
#include "log.h"

OpticalLinkPowerModel::OpticalLinkPowerModel(volatile float link_frequency, \
      volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints):
   OpticalLinkModel(link_frequency, link_length, link_width, num_receiver_endpoints),
   LinkPowerModel()
{
   try
   {
      // Static Power parameters
      _ring_tuning_power = Sim()->getCfg()->getFloat("link_model/optical/power/static/ring_tuning_power");
      _laser_power = Sim()->getCfg()->getFloat("link_model/optical/power/static/laser_power");
      _electrical_tx_static_power = Sim()->getCfg()->getFloat("link_model/optical/power/static/electrical_tx_power");
      _electrical_rx_static_power = Sim()->getCfg()->getFloat("link_model/optical/power/static/electrical_rx_power");
      _clock_static_power_tx = Sim()->getCfg()->getFloat("link_model/optical/power/fixed/clock_power_tx");
      _clock_static_power_rx = Sim()->getCfg()->getFloat("link_model/optical/power/fixed/clock_power_rx");

      // Dynamic Power parameters
      _electrical_tx_dynamic_energy = Sim()->getCfg()->getFloat("link_model/optical/power/dynamic/electrical_tx_energy");
      _electrical_rx_dynamic_energy = Sim()->getCfg()->getFloat("link_model/optical/power/dynamic/electrical_rx_energy");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read optical link parameters from the cfg file");
   }
}

OpticalLinkPowerModel::~OpticalLinkPowerModel()
{}

volatile double
OpticalLinkPowerModel::getLaserPower()
{
   return ( _link_width * \
            ( \
              _laser_power \
            ) \
          );
}

volatile double
OpticalLinkPowerModel::getRingTuningPower()
{
   return ( _link_width * \
            ( \
              _ring_tuning_power + \
              _num_receiver_endpoints * \
              ( \
                _ring_tuning_power \
              ) \
            ) \
          );
}

volatile double
OpticalLinkPowerModel::getStaticPower()
{
   return ( _link_width * \
            ( \
               _laser_power + \
               _ring_tuning_power + _electrical_tx_static_power + _clock_static_power_tx + \
               _num_receiver_endpoints * \
               ( \
                  _ring_tuning_power + _electrical_rx_static_power + _clock_static_power_rx \
               ) \
            ) \
          );
}

volatile double
OpticalLinkPowerModel::getDynamicEnergySender()
{
   return _total_dynamic_energy_sender;
}

volatile double
OpticalLinkPowerModel::getDynamicEnergyReceiver()
{
   return _total_dynamic_energy_receiver;
}

void
OpticalLinkPowerModel::updateDynamicEnergy(UInt32 num_bit_flips, UInt32 num_flits)
{
   _total_dynamic_energy_sender += (num_flits * (num_bit_flips * (_electrical_tx_dynamic_energy)));
   _total_dynamic_energy_receiver += (num_flits * (num_bit_flips * (_electrical_rx_dynamic_energy * _num_receiver_endpoints)));
}
