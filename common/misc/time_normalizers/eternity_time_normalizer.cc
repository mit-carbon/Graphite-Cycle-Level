#include "eternity_time_normalizer.h"
#include "utils.h"
#include "log.h"

EternityTimeNormalizer::EternityTimeNormalizer(SInt32 num_entities):
   TimeNormalizer(num_entities)
{
   _last_normalized_time = 0.0;
   _start_wall_clock_time = rdtscll();
   _average_rate_of_progress = 0.0;

   _rate_of_progress_list = new double[_num_entities];
   for (SInt32 i = 0; i < _num_entities; i++)
   {
      _rate_of_progress_list[i] = 0.0;
   }

   // Accuracy Counters
   _total_requests = 0;
   _total_mispredicted = 0;
}

EternityTimeNormalizer::~EternityTimeNormalizer()
{
   LOG_ASSERT_ERROR(_num_active_entities == _num_entities,
         "num_active_entities(%i), num_entities(%i)", _num_active_entities, _num_entities);
   delete [] _rate_of_progress_list;
}

UInt64
EternityTimeNormalizer::normalize(UInt64 simulated_time, SInt32 entity_id)
{
   ScopedLock sl(_lock);

   UInt64 wall_clock_time = rdtscll();

   volatile double rate_of_progress = ((double) simulated_time) / \
                                      ((double) (wall_clock_time - _start_wall_clock_time));
   assert(rate_of_progress > 0);

   SInt32 prev_num_active_entities = _num_active_entities;
   if (_rate_of_progress_list[entity_id] == 0.0)
      _num_active_entities ++;
   SInt32 curr_num_active_entities = _num_active_entities;

   _average_rate_of_progress = (_average_rate_of_progress * prev_num_active_entities + \
                               rate_of_progress - _rate_of_progress_list[entity_id]) / curr_num_active_entities;
   _rate_of_progress_list[entity_id] = rate_of_progress;

   double predicted_normalized_time = _average_rate_of_progress * ((double) (wall_clock_time - _start_wall_clock_time));
   if (predicted_normalized_time < _last_normalized_time)
      _total_mispredicted ++;
   _total_requests ++;

   double normalized_time = getMax<double>(_last_normalized_time, predicted_normalized_time);

   LOG_PRINT("Simulated Time(%llu), Wall Clock Time(%llu), entity id(%i), Prev Normalized Time(%llu), Normalized Time(%llu)", \
         simulated_time, wall_clock_time, entity_id, (UInt64) _last_normalized_time, (UInt64) normalized_time);

   // Update Variables
   _last_normalized_time = normalized_time;

   return (UInt64) _last_normalized_time;
}

UInt64
EternityTimeNormalizer::renormalize(UInt64 simulated_time, double average_rate_of_progress)
{
   ScopedLock sl(_lock);

   return (UInt64) ((_average_rate_of_progress / average_rate_of_progress) * ((double) simulated_time));
}
