#define __STDC_LIMIT_MACROS
#include <climits>

#include "meta_event_heap.h"
#include "log.h"

MetaEventHeap::MetaEventHeap(SInt32 num_children, MetaEventHeap* parent_event_heap, \
      SInt32 event_heap_index_in_parent):
   _first_event_time(UINT64_MAX),
   _parent_event_heap(parent_event_heap),
   _event_heap_index_in_parent(event_heap_index_in_parent)
{
   for (SInt32 i = 0; i < num_children; i++)
   {
      _heap_nodes.push_back(new MinHeap::Node(UINT64_MAX));
      _heap.insert(_heap_nodes.back());
   }
}

MetaEventHeap::~MetaEventHeap()
{
   while (!_heap_nodes.empty())
   {
      delete _heap_nodes.back();
      _heap_nodes.pop_back();
   }
}

void
MetaEventHeap::updateTime(SInt32 event_index, UInt64 time)
{
   _lock.acquire();

   LOG_PRINT("MetaEventHeap(%p): updateTime(%i, %llu)", this, event_index, time);   
   bool top_of_heap_change = _heap.updateKey(_heap_nodes[event_index], time);
   if (top_of_heap_change)
   {
      UInt64 next_event_time = (_heap.min()).first;
      if (_parent_event_heap)
         _parent_event_heap->updateTime(_event_heap_index_in_parent, next_event_time);
      
      _first_event_time = next_event_time;
   }

   _lock.release();
}
