#include <cassert>

#include "simulator.h"
#include "event_manager.h"
#include "event_heap.h"
#include "meta_event_heap.h"
#include "event_queue_manager.h"

using std::make_pair;

EventHeap::EventHeap(EventQueueManager* event_queue_manager,
      MetaEventHeap* parent_event_heap, SInt32 event_heap_index_in_parent):
   EventQueue(event_queue_manager),
   _first_event_time(UINT64_MAX_),
   _parent_event_heap(parent_event_heap),
   _event_heap_index_in_parent(event_heap_index_in_parent)
{
   // At initialization, leaf_event_heap has no events
   _heap.insert(UINT64_MAX_, NULL);
}

EventHeap::~EventHeap()
{
   assert(_heap.size() == 1);
   assert( _heap.min() == (make_pair<UInt64,void*>(UINT64_MAX_,NULL)) );
}

void
EventHeap::push(Event* event, bool is_locked)
{
   LOG_PRINT("EventHeap(%i): push(Event[%p],Type[%u],Time[%llu]), is_locked(%s) enter",
         getEventQueueManager()->getId(), event, event->getType(), event->getTime(), is_locked ? "YES" : "NO");

   if (!is_locked)
      _lock.acquire();

   // Insert new packet into event queue
   // If new event = most recent event, update the sim_thread_time_heap also
   bool top_of_heap_change = _heap.insert(event->getTime(), (void*) event);
   if (top_of_heap_change)
   {
      UInt64 next_event_time = (_heap.min()).first;
      assert(next_event_time == event->getTime());

      // Update parent meta_event_heap
      _parent_event_heap->updateTime(_event_heap_index_in_parent, next_event_time, is_locked);
     
      // Update _first_event_time 
      _first_event_time = next_event_time;
      
      // Signal Event (Make this more conservative later)
      getEventQueueManager()->signalEvent();
   }

   if (!is_locked)
      _lock.release();
   
   LOG_PRINT("EventHeap(%i): push(Event[%p],Type[%u],Time[%llu]), is_locked(%s) exit",
         getEventQueueManager()->getId(), event, event->getType(), event->getTime(), is_locked ? "YES" : "NO");
}

void
EventHeap::processEvents()
{
   _lock.acquire();
   
   LOG_PRINT("EventHeap(%i): processEvents(First Time[%llu]), Size(%u) enter",
         getEventQueueManager()->getId(), _first_event_time, _heap.size());
   
   while (Sim()->getEventManager()->isReady(_first_event_time))
   {
      // Process the event at the top of the heap
      Event* event = (Event*) (_heap.min()).second;
      LOG_ASSERT_ERROR(event && (Sim()->getEventManager()->isReady(event->getTime())),
            "event(%p)", event);

      _lock.release();

      // Network, Instruction, Memory Modeling 
      event->process();
      
      _lock.acquire();

      // Remove the event from the top of the heap
      Event* curr_event = (Event*) ((_heap.extractMin()).second);
      LOG_ASSERT_ERROR(event == curr_event, "Old Event(%p)[%llu,%i], New Event(%p)[%llu,%i]",
            event, event->getTime(), event->getType(),
            curr_event, curr_event->getTime(), curr_event->getType());
      
      // Get next event in order of time
      Event* next_event = (Event*) ((_heap.min()).second);
      UInt64 next_event_time = (next_event) ? next_event->getTime() : UINT64_MAX_;
      Event::Type next_event_type = (next_event) ? next_event->getType() : Event::INVALID;

      LOG_PRINT("EventHeap(%i): After extractMin(), Next Event (Type[%u],Time[%llu])",
            getEventQueueManager()->getId(), next_event_type, next_event_time);

      LOG_ASSERT_ERROR(next_event_time >= event->getTime(), "Next Event Time(%llu), Curr Event Time(%llu)",
            next_event_time, event->getTime());

      if (next_event_time > event->getTime())
      {
         // Update Local Time - Global Time is always updated since top of heap changes
         _parent_event_heap->updateTime(_event_heap_index_in_parent, next_event_time);

         // Update _first_event_time
         _first_event_time = next_event_time;

         // Wake up others(/sim_threads) who are sleeping who have ready events
         Sim()->getEventManager()->wakeUpWaiters();
      }

      // Delete the Curr Event
      delete event;
   }
      
   LOG_PRINT("EventHeap(%i): processEvents(First Time[%llu]), Size(%u) exit",
         getEventQueueManager()->getId(), _first_event_time, _heap.size());
   
   _lock.release(); 
}
