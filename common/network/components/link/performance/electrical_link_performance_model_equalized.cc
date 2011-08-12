#include <cmath>

#include "electrical_link_performance_model_equalized.h"
#include "simulator.h"
#include "config.h"
#include "log.h"

ElectricalLinkPerformanceModelEqualized::ElectricalLinkPerformanceModelEqualized(volatile float link_frequency, \
      volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints):
   ElectricalLinkPerformanceModel(link_frequency, link_length, link_width, num_receiver_endpoints) 
{
   try
   {
      // Delay Parameters
      _wire_delay_per_mm = Sim()->getCfg()->getFloat("link_model/electrical_equalized/delay/delay_per_mm");
      _tx_delay = Sim()->getCfg()->getInt("link_model/electrical_equalized/delay/tx_delay");
      _rx_delay = Sim()->getCfg()->getInt("link_model/electrical_equalized/delay/rx_delay");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Unable to read equalized electrical link parameters from the cfg file");
   }

   _net_link_delay = (UInt64) ceil((_wire_delay_per_mm * _link_length) * _link_frequency + \
                                    _tx_delay + \
                                    _rx_delay);
}

ElectricalLinkPerformanceModelEqualized::~ElectricalLinkPerformanceModelEqualized()
{}

UInt64
ElectricalLinkPerformanceModelEqualized::getDelay()
{
   return _net_link_delay;
}
