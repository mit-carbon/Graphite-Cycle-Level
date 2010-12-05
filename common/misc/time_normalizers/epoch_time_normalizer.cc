#include "epoch_time_normalizer.h"
#include "simulator.h"
#include "config.h"
#include "utils.h"
#include "log.h"

EpochTimeNormalizer::EpochTimeNormalizer(SInt32 num_entities):
   TimeNormalizer(num_entities)
{
   _window_size = Sim()->getCfg()->getInt("time_normalizer/epoch/window_size", 1);

   _last_normalized_time = 0;
   _start_wall_clock_time = rdtscll();
   _last_wall_clock_time = _start_wall_clock_time;
   _average_rate_of_progress = 0.0;
   _overall_average_rate_of_progress = 0.0;
   
   _simulated_time_list.resize(_num_entities);
   _wall_clock_time_list.resize(_num_entities);
   _rate_of_progress_list = new double[_num_entities];
   for (SInt32 i = 0; i < _num_entities; i++)
   {
      _simulated_time_list[i].push(0);
      _wall_clock_time_list[i].push(_start_wall_clock_time);
      _rate_of_progress_list[i] = 0.0;
   }
}

EpochTimeNormalizer::~EpochTimeNormalizer()
{
   assert(_num_active_entities == _num_entities);
   delete [] _rate_of_progress_list;
}

UInt64
EpochTimeNormalizer::normalize(UInt64 simulated_time, SInt32 entity_id)
{
   ScopedLock sl(_lock);

   UInt64 wall_clock_time = rdtscll();

   LOG_ASSERT_ERROR(simulated_time > _simulated_time_list[entity_id].back(), \
         "simulated time(%llu), prev simulated time(%llu), entity id(%i)", \
         simulated_time, _simulated_time_list[entity_id].back(), entity_id);

   // Gives the instantaneous rate of progress as opposed to the rate of progress since start of simulation
   volatile double rate_of_progress = ((double) (simulated_time - _simulated_time_list[entity_id].front())) \
                                      / ((double) (wall_clock_time - _wall_clock_time_list[entity_id].front()));

   /*
   printf("BEFORE: Simulated Time(%llu), Wall Clock Time(%llu), Entity Id(%i): Last(G) Normalized Time(%llu), Last(G) Wall Clock Time(%llu), Average Rate of Progress(%g), Last Simulated Time(%llu), Last Wall Clock Time(%llu), Rate of Progress(%g)\n",
         (long long unsigned int) simulated_time,
         (long long unsigned int) wall_clock_time,
         entity_id,
         (long long unsigned int) _last_normalized_time,
         (long long unsigned int) _last_wall_clock_time,
         _average_rate_of_progress,
         (long long unsigned int) _simulated_time_list[entity_id].front(),
         (long long unsigned int) _wall_clock_time_list[entity_id].front(),
         rate_of_progress);
    */

   SInt32 prev_num_active_entities = _num_active_entities;
   if (_rate_of_progress_list[entity_id] == 0)
      _num_active_entities ++;
   SInt32 curr_num_active_entities = _num_active_entities;

   _average_rate_of_progress = (_average_rate_of_progress * prev_num_active_entities + \
                               rate_of_progress - _rate_of_progress_list[entity_id]) / curr_num_active_entities;
   _rate_of_progress_list[entity_id] = rate_of_progress;

   volatile double normalized_time = _last_normalized_time + \
                                     (_average_rate_of_progress * ((double) (wall_clock_time - _last_wall_clock_time)));
  
   // Update Variables 
   _last_normalized_time = normalized_time;
   _last_wall_clock_time = wall_clock_time;
   
   _simulated_time_list[entity_id].push(simulated_time);
   _wall_clock_time_list[entity_id].push(wall_clock_time);
   if (_simulated_time_list[entity_id].size() > (UInt32) _window_size)
   {
      _simulated_time_list[entity_id].pop();
      _wall_clock_time_list[entity_id].pop();
   }

   /*
   printf("AFTER: Normalized Time(%llu), Average Rate of Progress(%g)\n", \
         (long long unsigned int) normalized_time, _average_rate_of_progress);
    */

   return (UInt64) normalized_time;
}

UInt64
EpochTimeNormalizer::renormalize(UInt64 simulated_time, double average_rate_of_progress)
{
   ScopedLock sl(_lock);

   // Should have a global average rate of progress
   return (UInt64) ((_overall_average_rate_of_progress / average_rate_of_progress) * ((double) simulated_time));
}
