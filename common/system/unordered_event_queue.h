#pragma once

#include <queue>
using std::queue;

#include "event_queue.h"
#include "lock.h"

class UnorderedEventQueue : public EventQueue
{
   public:
      UnorderedEventQueue(EventQueueManager* event_queue_manager);
      ~UnorderedEventQueue();

      void processEvents();
      void push(Event* event, bool is_locked = false);
      void acquireLock() { _lock.acquire(); }
      void releaseLock() { _lock.release(); }

      bool empty() { return _queue.empty(); }

   private:
      queue<Event*> _queue;
      Lock _lock;
};
