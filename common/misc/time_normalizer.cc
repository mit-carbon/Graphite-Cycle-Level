#include "time_normalizer.h"
#include "utils.h"
#include "log.h"

TimeNormalizer::TimeNormalizer(SInt32 num_entities):
   _num_entities(num_entities)
{
   _simulated_time_list.resize(_num_entities);
   _wall_clock_time_list.resize(_num_entities);
   for (SInt32 i = 0; i < _num_entities; i++)
   {
      _simulated_time_list[i] = 0;
      _wall_clock_time_list[i] = rdtscll();
   }

   _last_normalized_time = 0;
   _last_wall_clock_time = rdtscll();
}

TimeNormalizer::~TimeNormalizer()
{}

UInt64
TimeNormalizer::normalize(UInt64 simulated_time, SInt32 entity_id)
{
   return simulated_time;
   /*
   UInt64 wall_clock_time = rdtscll();

   simulated_time = max<UInt64>(simulated_time, _simulated_time_list[entity_id]);
   volatile double rate_of_progress = ((double) (simulated_time - _simulated_time_list[entity_id])) \
                                      / (wall_clock_time - _wall_clock_time_list[entity_id]);
   
   UInt64 normalized_time = _last_normalized_time + rate_of_progress * (wall_clock_time - _last_wall_clock_time);
   
   // Update variables
   _last_normalized_time = normalized_time;
   _last_wall_clock_time = wall_clock_time;
   _simulated_time_list[entity_id] = simulated_time;
   _wall_clock_time_list[entity_id] = wall_clock_time;

   LOG_PRINT("normalizeTime(%llu, %i) -> %llu", simulated_time, entity_id, normalized_time);
   return normalized_time;
    */
}
