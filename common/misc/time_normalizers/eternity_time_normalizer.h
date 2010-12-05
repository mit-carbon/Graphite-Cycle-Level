#include <stdio.h>

#include "time_normalizer.h"

class EternityTimeNormalizer : public TimeNormalizer
{
   public:
      EternityTimeNormalizer(SInt32 num_entities);
      ~EternityTimeNormalizer();

      UInt64 normalize(UInt64 simulated_time, SInt32 entity_id);
      UInt64 renormalize(UInt64 simulated_time, double average_rate_of_progress);
      volatile double getAverageRateOfProgress()
      { return _average_rate_of_progress; }
      UInt64 getTotalRequests() { return _total_requests; }
      UInt64 getTotalMispredicted() { return _total_mispredicted; }

   private:
      volatile double _last_normalized_time;
      UInt64 _start_wall_clock_time;
      volatile double _average_rate_of_progress;

      volatile double* _rate_of_progress_list;

      // Accuracy Counters
      UInt64 _total_requests;
      UInt64 _total_mispredicted;
};
