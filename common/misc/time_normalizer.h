#pragma once

#include <vector>
using namespace std;

#include "fixed_types.h"

class TimeNormalizer
{
   public:
      TimeNormalizer(SInt32 num_entities);
      ~TimeNormalizer();

      UInt64 normalize(UInt64 simulated_time, SInt32 entity_id);

   private:
      vector<UInt64> _simulated_time_list;
      vector<UInt64> _wall_clock_time_list;

      UInt64 _last_normalized_time;
      UInt64 _last_wall_clock_time;

      SInt32 _num_entities;
};
