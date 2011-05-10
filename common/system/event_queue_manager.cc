#include "event_queue_manager.h"
#include "event_heap.h"
#include "unordered_event_queue.h"
#include "simulator.h"
#include "event_manager.h"
#include "log.h"

EventQueueManager::EventQueueManager(SInt32 id):
   _id(id)
{}

EventQueueManager::~EventQueueManager()
{}

void
EventQueueManager::setEventQueues(EventHeap* event_heap, UnorderedEventQueue* unordered_event_queue)
{
   _event_heap = event_heap;
   _unordered_event_queue = unordered_event_queue;
}

EventQueue*
EventQueueManager::getEventQueue(EventQueue::Type type)
{
   if (type == EventQueue::ORDERED)
      return _event_heap;
   else
      return _unordered_event_queue;
}

void
EventQueueManager::processEvents()
{
   LOG_PRINT("EventQueueManager(%i): processEvents() enter", getId());
   _binary_semaphore.wait();
   _event_heap->processEvents();
   _unordered_event_queue->processEvents();
   LOG_PRINT("EventQueueManager(%i): processEvents() exit", getId());
}

void
EventQueueManager::signalEvent()
{
   LOG_PRINT("EventQueueManager(%i): signalEvent() enter", getId());
   if ( (Sim()->getEventManager()->isReady(_event_heap->getFirstEventTime()))
         || (!_unordered_event_queue->empty()) )
   {
      LOG_PRINT("Signaled Event");
      _binary_semaphore.signal();
   }
   LOG_PRINT("EventQueueManager(%i): signalEvent() exit", getId());
}
