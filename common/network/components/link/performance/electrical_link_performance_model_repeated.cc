#include <cmath>

#include "electrical_link_performance_model_repeated.h"
#include "simulator.h"
#include "config.h"
#include "log.h"

ElectricalLinkPerformanceModelRepeated::ElectricalLinkPerformanceModelRepeated(volatile float link_frequency,
      volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints):
   ElectricalLinkPerformanceModel(link_frequency, link_length, link_width, num_receiver_endpoints)
{
   try
   {
      // Delay Parameters
      _delay_per_mm = Sim()->getCfg()->getFloat("link_model/electrical_repeated/delay/delay_per_mm");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Unable to read repeated electrical link parameters from the cfg file");
   }
   
   _net_link_delay = (UInt64) ceil(_delay_per_mm * _link_length);
}

ElectricalLinkPerformanceModelRepeated::~ElectricalLinkPerformanceModelRepeated()
{}

UInt64
ElectricalLinkPerformanceModelRepeated::getDelay()
{
   return _net_link_delay;
}
