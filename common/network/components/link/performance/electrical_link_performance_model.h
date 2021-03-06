#pragma once

#include <string>
using namespace std;

#include "electrical_link_model.h"
#include "link_performance_model.h"
#include "fixed_types.h"

class ElectricalLinkPerformanceModel : public ElectricalLinkModel, public LinkPerformanceModel
{
public:
   ElectricalLinkPerformanceModel(volatile float link_frequency, volatile double link_length, \
         UInt32 link_width, SInt32 num_receiver_endpoints);
   virtual ~ElectricalLinkPerformanceModel();

   static ElectricalLinkPerformanceModel* create(string link_type_str, \
         volatile float link_frequency, volatile double link_length, UInt32 link_width, \
         SInt32 num_receiver_endpoints);
};
