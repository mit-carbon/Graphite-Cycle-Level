#include <assert.h>
#include "queue_model_simple.h"
#include "utils.h"
#include "log.h"

QueueModelSimple::QueueModelSimple(bool check)
   : _queue_time(0), _last_event_time(0), _check(check)
{}

QueueModelSimple::~QueueModelSimple()
{}

UInt64
QueueModelSimple::computeQueueDelay(UInt64 event_time, UInt64 processing_time, core_id_t requester)
{
   LOG_PRINT("EventTime(%llu), LastEventTime(%llu)", event_time, _last_event_time);
   
   if (_check)
   {
      // LOG_ASSERT_ERROR(event_time >= _last_event_time, "EventTime(%llu), LastEventTime(%llu)",
      //       event_time, _last_event_time);
   }
   
   // Update _last_event_time : for verification purposes
   _last_event_time = event_time;

   // Compute the delay
   UInt64 delay = (_queue_time > event_time) ? (_queue_time - event_time) : 0;
   _queue_time = getMax<UInt64>(event_time, _queue_time) + processing_time;
   
   return delay;
}
