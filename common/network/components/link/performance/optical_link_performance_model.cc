#include <cmath>

#include "optical_link_performance_model.h"
#include "simulator.h"
#include "config.h"
#include "log.h"

OpticalLinkPerformanceModel::OpticalLinkPerformanceModel(volatile float link_frequency, \
      volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints):
   OpticalLinkModel(link_frequency, link_length, link_width, num_receiver_endpoints),
   LinkPerformanceModel()
{
   try
   {
      // Delay Parameters
      _waveguide_delay_per_mm = Sim()->getCfg()->getFloat("link_model/optical/delay/waveguide_delay_per_mm");
      _e_o_conversion_delay = Sim()->getCfg()->getInt("link_model/optical/delay/E-O_conversion");
      _o_e_conversion_delay = Sim()->getCfg()->getInt("link_model/optical/delay/O-E_conversion");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read optical link parameters from the cfg file");
   }

   // _net_optical_link_delay is in clock cycles
   _net_optical_link_delay = (UInt64) (ceil( \
                                          (_waveguide_delay_per_mm * _link_length) * _link_frequency + \
                                          _e_o_conversion_delay + \
                                          _o_e_conversion_delay \
                                        ));
}

OpticalLinkPerformanceModel::~OpticalLinkPerformanceModel()
{}

UInt64
OpticalLinkPerformanceModel::getDelay()
{
   return _net_optical_link_delay;
}
