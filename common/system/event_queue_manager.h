#pragma once

#include <stack>
using std::stack;

#include "lock.h"
#include "binary_semaphore.h"
#include "event.h"
#include "event_queue.h"

class EventHeap;
class UnorderedEventQueue;

class EventQueueManager
{
public:
   EventQueueManager(SInt32 id);
   ~EventQueueManager();

   SInt32 getId() { return _id; }

   // Polling, Waiting and Signalling an event
   void processEvents();
   void signalEvent();

   void setEventQueues(EventHeap* event_heap, UnorderedEventQueue* unordered_event_queue);
   EventQueue* getEventQueue(EventQueue::Type type);

private:
   SInt32 _id;

   // Consists of
   // 1) EventHeap (aka OrderedEventQueue)
   // 2) UnorderedEventQueue
   EventHeap* _event_heap;
   UnorderedEventQueue* _unordered_event_queue;
   
   // Synchronization between event queues
   BinarySemaphore _binary_semaphore;
};
