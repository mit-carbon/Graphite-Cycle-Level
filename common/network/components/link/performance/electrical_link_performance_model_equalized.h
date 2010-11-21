#pragma once

#include "electrical_link_performance_model.h"
#include "fixed_types.h"

class ElectricalLinkPerformanceModelEqualized : public ElectricalLinkPerformanceModel
{
public:
   ElectricalLinkPerformanceModelEqualized(volatile float link_frequency, \
         volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints);
   ~ElectricalLinkPerformanceModelEqualized();

   UInt64 getDelay();

private:
   // Delay Parameters
   volatile double _wire_delay_per_mm;
   UInt64 _tx_delay;
   UInt64 _rx_delay;
   UInt64 _net_link_delay; 
};
