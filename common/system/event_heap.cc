#define __STDC_LIMIT_MACROS
#include <climits>
#include <cassert>

#include "simulator.h"
#include "event_manager.h"
#include "event_heap.h"
#include "meta_event_heap.h"
#include "event_queue_manager.h"

using std::make_pair;

EventHeap::EventHeap(EventQueueManager* event_queue_manager, \
      MetaEventHeap* parent_event_heap, SInt32 event_heap_index_in_parent):
   EventQueue(event_queue_manager),
   _first_event_time(UINT64_MAX),
   _parent_event_heap(parent_event_heap),
   _event_heap_index_in_parent(event_heap_index_in_parent)
{
   // At initialization, leaf_event_heap has no events
   _heap.insert(UINT64_MAX, NULL);
}

EventHeap::~EventHeap()
{
   assert(_heap.size() == 1);
   assert( _heap.min() == (make_pair<UInt64,void*>(UINT64_MAX,NULL)) );
}

void
EventHeap::push(Event* event)
{
   _lock.acquire();
   LOG_PRINT("EventHeap(%i): push(%llu)", getEventQueueManager()->getId(), event->getTime());

   // Insert new packet into event queue
   // If new event = most recent event, update the sim_thread_time_heap also
   bool top_of_heap_change = _heap.insert(event->getTime(), (void*) event);
   if (top_of_heap_change)
   {
      UInt64 next_event_time = (_heap.min()).first;
      assert(next_event_time == event->getTime());

      // Update parent meta_event_heap
      _parent_event_heap->updateTime(_event_heap_index_in_parent, next_event_time);
     
      // Update _first_event_time 
      _first_event_time = next_event_time;
      
      // Signal Event (Make this more conservative later)
      getEventQueueManager()->signalEvent();
   }

   _lock.release();
}

void
EventHeap::processEvents()
{
   LOG_PRINT("First Event Time on Entry(%llu) - (%p)", _first_event_time, &_first_event_time);
   while (Sim()->getEventManager()->isReady(_first_event_time))
   {
      _lock.acquire();

      // Process the event at the top of the heap
      Event* event = (Event*) (_heap.min()).second;
      LOG_ASSERT_ERROR(event && (Sim()->getEventManager()->isReady(event->getTime())),
            "event(%p)", event);

      _lock.release();

      // Network, Instruction, Memory Modeling 
      event->process();
      delete event;
      
      _lock.acquire();

      // Remove the event from the top of the heap
      Event* curr_event = (Event*) ((_heap.extractMin()).second);
      LOG_ASSERT_ERROR(event == curr_event, "event(%p), curr_event(%p)", event, curr_event);
      
      // Get next event in order of time
      Event* next_event = (Event*) ((_heap.min()).second);
      UInt64 next_event_time = (next_event) ? next_event->getTime() : UINT64_MAX;
      LOG_PRINT("EventHeap(%i): After extractMin(), Next Event Time(%llu)", \
            getEventQueueManager()->getId(), next_event_time);

      // Update Local Time - Global Time is always updated since top of heap changes
      _parent_event_heap->updateTime(_event_heap_index_in_parent, next_event_time);

      // Update _first_event_time
      _first_event_time = next_event_time;

      _lock.release();

      // Wake up others(/sim_threads) who are sleeping who have ready events
      Sim()->getEventManager()->wakeUpWaiters();
   }
   LOG_PRINT("First Event Time on Exit(%llu) - (%p)", _first_event_time, &_first_event_time);
}
