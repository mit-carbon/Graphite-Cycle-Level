#include <vector>
#include <queue>
using std::vector;
using std::queue;

#include "time_normalizer.h"

class EpochTimeNormalizer : public TimeNormalizer
{
   public:
      EpochTimeNormalizer(SInt32 num_entities);
      ~EpochTimeNormalizer();

      UInt64 normalize(UInt64 simulated_time, SInt32 entity_id);
      UInt64 renormalize(UInt64 simulated_time, double average_rate_of_progress);
      volatile double getAverageRateOfProgress()
      { return _overall_average_rate_of_progress; }
      UInt64 getTotalRequests() { return 0; }
      UInt64 getTotalMispredicted() { return 0; }

   private:
      volatile double _last_normalized_time;
      UInt64 _start_wall_clock_time;
      UInt64 _last_wall_clock_time;
      volatile double _average_rate_of_progress;
      volatile double _overall_average_rate_of_progress;
      
      vector<queue<UInt64> > _simulated_time_list;
      vector<queue<UInt64> > _wall_clock_time_list;
      volatile double* _rate_of_progress_list;

      SInt32 _window_size;
};
