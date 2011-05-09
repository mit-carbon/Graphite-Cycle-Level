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
MetaEventHeap::updateTime(SInt32 event_index, UInt64 time, bool is_locked)
{
   LOG_PRINT("MetaEventHeap: updateTime(Event Queue Index[%i], Time[%llu]), is_locked(%s) enter", \
         event_index, time, is_locked ? "YES" : "NO");

   if (!is_locked)
      _lock.acquire();

   UInt64 initial_time = _first_event_time;

   LOG_PRINT("First Event Time(%llu)", _first_event_time);

   bool top_of_heap_change = _heap.updateKey(_heap_nodes[event_index], time);
   if (top_of_heap_change)
   {
      LOG_PRINT("Top of Heap Changed");

      UInt64 next_event_time = (_heap.min()).first;
      if (_parent_event_heap)
         _parent_event_heap->updateTime(_event_heap_index_in_parent, next_event_time, is_locked);
      
      _first_event_time = next_event_time;

      UInt64 final_time = _first_event_time;
      if (!_parent_event_heap)
      {
         LOG_ASSERT_ERROR((initial_time == UINT64_MAX) || (final_time > initial_time),
               "Initial Time(%llu), Final Time(%llu)", initial_time, final_time);
      }
   }

   if (!is_locked)
      _lock.release();
   
   LOG_PRINT("MetaEventHeap: updateTime(Event Queue Index[%i], Time[%llu]), is_locked(%s) exit", \
         event_index, time, is_locked ? "YES" : "NO");
}
