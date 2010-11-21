#pragma once

#include <string>
using namespace std;

#include "electrical_link_model.h"
#include "link_power_model.h"
#include "fixed_types.h"

class ElectricalLinkPowerModel : public ElectricalLinkModel, public LinkPowerModel
{
public:
   ElectricalLinkPowerModel(volatile float link_frequency, volatile double link_length, \
         UInt32 link_width, SInt32 num_receiver_endpoints);
   virtual ~ElectricalLinkPowerModel();

   static ElectricalLinkPowerModel* create(string link_type_str, \
         volatile float link_frequency, volatile double link_length, UInt32 link_width, \
         SInt32 num_receiver_endpoints, bool use_orion = true);
};
