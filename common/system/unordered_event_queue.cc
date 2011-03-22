#include "unordered_event_queue.h"
#include "event_queue_manager.h"

UnorderedEventQueue::UnorderedEventQueue(EventQueueManager* event_queue_manager):
   EventQueue(event_queue_manager)
{}

UnorderedEventQueue::~UnorderedEventQueue()
{}

void
UnorderedEventQueue::processEvents()
{
   _lock.acquire();
   while(!_queue.empty())
   {
      Event* event = _queue.front();
      _queue.pop();
      _lock.release();

      event->process();
      delete event;

      _lock.acquire();
   }
   _lock.release();
}

void
UnorderedEventQueue::push(Event* event)
{
   _lock.acquire();
   _queue.push(event);
   _lock.release();

   getEventQueueManager()->signalEvent();
}
