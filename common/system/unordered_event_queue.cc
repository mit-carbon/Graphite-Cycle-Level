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
   LOG_PRINT("UnorderedEventQueue(%i): processEvents() enter", getEventQueueManager()->getId());
   
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
   
   LOG_PRINT("UnorderedEventQueue(%i): processEvents() exit", getEventQueueManager()->getId());
}

void
UnorderedEventQueue::push(Event* event, bool is_locked)
{
   LOG_PRINT("UnorderedEventQueue(%i): push(Event[%p],Type[%u],Time[%llu]), is_locked(%s) enter", \
         getEventQueueManager()->getId(), event, event->getType(), event->getTime(), is_locked ? "YES" : "NO");

   assert(!is_locked);
   _lock.acquire();
   _queue.push(event);
   _lock.release();

   getEventQueueManager()->signalEvent();
   
   LOG_PRINT("UnorderedEventQueue(%i): push(Event[%p],Type[%u],Time[%llu]), is_locked(%s) exit", \
         getEventQueueManager()->getId(), event, event->getType(), event->getTime(), is_locked ? "YES" : "NO");
}
