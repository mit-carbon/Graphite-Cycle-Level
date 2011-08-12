#pragma once

#include <string>

#include "fixed_types.h"

class LinkPowerModel
{
public:
   LinkPowerModel() {}
   virtual ~LinkPowerModel() {}

   virtual volatile double getStaticPower() = 0;
   virtual void updateDynamicEnergy(UInt32 num_bit_flips, UInt32 num_flits = 1) = 0;
   virtual volatile double getDynamicEnergy() = 0;

   virtual void resetCounters() = 0;
};
