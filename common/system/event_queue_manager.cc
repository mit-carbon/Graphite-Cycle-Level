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
   LOG_PRINT("EventQueueManager::processEvents() wait starting");
   _binary_semaphore.wait();
   LOG_PRINT("EventQueueManager::processEvents() wait finished");
   _event_heap->processEvents();
   _unordered_event_queue->processEvents();
}

void
EventQueueManager::signalEvent()
{
   if ( (Sim()->getEventManager()->isReady(_event_heap->getFirstEventTime()))
         || (!_unordered_event_queue->empty()) )
   {
      _binary_semaphore.signal();
   }
}
