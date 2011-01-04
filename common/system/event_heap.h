#pragma once

#include "fixed_types.h"
#include "lock.h"
#include "binary_semaphore.h"
#include "meta_event_heap.h"
#include "min_heap.h"
#include "event.h"

class EventHeap
{
   public:
      EventHeap(SInt32 id, MetaEventHeap* parent_event_heap, SInt32 event_heap_index_in_parent);
      ~EventHeap();

      // Get the event heap id
      SInt32 getId() { return _id; }

      // First Event Time
      UInt64 getFirstEventTime() { return _first_event_time; }

      // enqueue the packet based on its time
      void push(Event* event);
      // get packet with the minimum time: dequeue may include waiting for the packet
      void poll();

      // Wait for an event to be ready
      void waitForEvent();
      // Signal that an event is ready
      void signalEvent();
 
   private:
      SInt32 _id;

      // Timestamp of the most recent event
      UInt64 _first_event_time;
      MinHeap _heap;

      // Locking the event heap
      Lock _lock;
      // For waiting on and signaling an event that is ready
      BinarySemaphore _binary_semaphore;
      
      // Pointer to Parent & Event Queue index in the parent's event queue 
      MetaEventHeap* _parent_event_heap;
      SInt32 _event_heap_index_in_parent;
};
