#pragma once

#include <vector>
using namespace std;

#include "fixed_types.h"
#include "config.h"
#include "lock.h"

class TimeNormalizer
{
   public:
      TimeNormalizer(SInt32 num_entities):
         _num_entities(num_entities), _num_active_entities(0) {}
      virtual ~TimeNormalizer() {}
      
      virtual UInt64 normalize(UInt64 simulated_time, SInt32 entity_id) = 0;
      virtual UInt64 renormalize(UInt64 simulated_time, double average_rate_of_progress) = 0;
      virtual volatile double getAverageRateOfProgress() = 0;
      virtual UInt64 getTotalRequests() = 0;
      virtual UInt64 getTotalMispredicted() = 0;

      static TimeNormalizer* create(SInt32 num_entities);
   
   protected:
      SInt32 _num_entities;
      SInt32 _num_active_entities;
     
      Lock _lock; 
};
