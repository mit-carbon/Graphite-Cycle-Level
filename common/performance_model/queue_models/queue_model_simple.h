#pragma once

#include "queue_model.h"

class QueueModelSimple : public QueueModel
{
public:
   QueueModelSimple(bool check=true);
   ~QueueModelSimple();

   UInt64 computeQueueDelay(UInt64 event_time, UInt64 processing_time, core_id_t requester = INVALID_CORE_ID);
private:
   UInt64 _queue_time;
   UInt64 _last_event_time;
   bool _check;
};

