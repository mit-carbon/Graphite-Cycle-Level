#define __STDC_LIMIT_MACROS
#include <climits>
#include <cassert>

#include "simulator.h"
#include "event_manager.h"
#include "event_heap.h"
#include "meta_event_heap.h"

using std::make_pair;

EventHeap::EventHeap(SInt32 id, MetaEventHeap* parent_event_heap, SInt32 event_heap_index_in_parent):
   _id(id),
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

   // Insert new packet into event queue
   // If new event = most recent event, update the sim_thread_time_heap also
   bool top_of_heap_change = _heap.insert(event->_time, (void*) event);
   if (top_of_heap_change)
   {
      _first_event_time = (_heap.min()).first;
      assert(_first_event_time == event->_time);
      _parent_event_heap->updateTime(_event_heap_index_in_parent, _first_event_time);
   }

   _lock.release();
}

void
EventHeap::poll()
{
   while(1)
   {
      waitForEvent();

      while (Sim()->getEventManager()->isReady(_first_event_time))
      {
         _lock.acquire();

         // Process the event at the top of the heap
         Event* event = (Event*) (_heap.min()).second;
         assert(Sim()->getEventManager()->isReady(event->_time));

         _lock.release();

         // Network, Instruction, Memory Modeling 
         event->process();

         _lock.acquire();

         // Remove the event from the top of the heap
         _heap.extractMin();
         
         // Get next event in order of time
         Event* next_event = (Event*) ((_heap.min()).second);
         _first_event_time = next_event->_time;

         // Update Local Time - Global Time is always updated since top of heap changes
         _parent_event_heap->updateTime(_event_heap_index_in_parent, next_event->_time);

         _lock.release();

         // Wake up others(/sim_threads) who are sleeping who have ready events
         Sim()->getEventManager()->wakeUpWaiters();
      }
   }
}

void
EventHeap::waitForEvent()
{
   _binary_semaphore.wait();
}

void
EventHeap::signalEvent()
{
   _binary_semaphore.signal();
}
