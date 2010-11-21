#pragma once

#include "electrical_link_performance_model.h"
#include "fixed_types.h"

class ElectricalLinkPerformanceModelRepeated : public ElectricalLinkPerformanceModel
{
public:
   ElectricalLinkPerformanceModelRepeated(volatile float link_frequency, \
         volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints);
   ~ElectricalLinkPerformanceModelRepeated();

   UInt64 getDelay();

private:
   // Delay Parameters
   volatile double _delay_per_mm;
   UInt64 _net_link_delay;
};
