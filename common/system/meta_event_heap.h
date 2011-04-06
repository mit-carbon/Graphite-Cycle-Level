#pragma once

#include <vector>
using std::vector;

#include "fixed_types.h"
#include "lock.h"
#include "min_heap.h"

class MetaEventHeap
{
   public:
      MetaEventHeap(SInt32 num_children, MetaEventHeap* parent_event_heap = NULL, \
            SInt32 event_heap_index_in_parent = -1);
      ~MetaEventHeap();
      
      UInt64 getFirstEventTime() { return _first_event_time; }

      // Update the time associated with an event. The event is addressed using its index
      // in the event queue
      void updateTime(SInt32 event_index, UInt64 time);

   private:
      // Timestamp of the most recent event
      volatile UInt64 _first_event_time;
      MinHeap _heap;

      // Locking the event heap
      Lock _lock;
      
      // Pointer to Parent & Event Queue index in the parent's event queue 
      MetaEventHeap* _parent_event_heap;
      SInt32 _event_heap_index_in_parent;
      
      // Event heap nodes corresponding to children
      // Each heap node has a time equal to the time corresponding to the top heap node in its children
      vector<MinHeap::Node*> _heap_nodes;
};
