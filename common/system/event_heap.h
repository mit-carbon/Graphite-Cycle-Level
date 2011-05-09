#pragma once

#include "event_queue.h"
#include "lock.h"
#include "meta_event_heap.h"
#include "min_heap.h"

class EventHeap : public EventQueue
{
   public:
      EventHeap(EventQueueManager* event_queue_manager,
            MetaEventHeap* parent_event_heap, SInt32 event_heap_index_in_parent);
      ~EventHeap();

      // First Event Time
      UInt64 getFirstEventTime() { return _first_event_time; }

      // get packet with the minimum time: dequeue may include waiting for the packet
      void processEvents();
      // enqueue the packet based on its time
      void push(Event* event, bool is_locked = false);
      // Acquire/Release Locks
      void acquireLock() { _lock.acquire(); }
      void releaseLock() { _lock.release(); }

   private:
      // Timestamp of the most recent event
      volatile UInt64 _first_event_time;
      MinHeap _heap;

      // Locking the event heap
      Lock _lock;
      
      // Pointer to Parent & Event Queue index in the parent's event queue 
      MetaEventHeap* _parent_event_heap;
      SInt32 _event_heap_index_in_parent;
};
