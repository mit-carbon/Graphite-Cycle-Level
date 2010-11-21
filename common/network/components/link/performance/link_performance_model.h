#pragma once

#include <string>

#include "fixed_types.h"

class LinkPerformanceModel
{
public:
   LinkPerformanceModel() {}
   virtual ~LinkPerformanceModel() {}

   virtual UInt64 getDelay() = 0;
};
