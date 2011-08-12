#pragma once

#include "optical_link_model.h"
#include "link_performance_model.h"
#include "fixed_types.h"

class OpticalLinkPerformanceModel : public OpticalLinkModel,  public LinkPerformanceModel
{
public:
   OpticalLinkPerformanceModel(volatile float link_frequency, \
         volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints);
   ~OpticalLinkPerformanceModel();

   UInt64 getDelay();

private:
   // Delay parameters
   volatile double _waveguide_delay_per_mm;
   UInt64 _e_o_conversion_delay;
   UInt64 _o_e_conversion_delay;

   UInt64 _net_optical_link_delay;
};
