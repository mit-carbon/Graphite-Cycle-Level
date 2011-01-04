#pragma once

#include <cassert>
#include <map>
#include <vector>
#include <stack>
using std::map;
using std::vector;
using std::stack;

#include "fixed_types.h"
#include "lock.h"
#include "event.h"
#include "event_heap.h"
#include "meta_event_heap.h"

class EventManager
{
   public:
      EventManager();
      ~EventManager();

      // Get the EventHeap corresponding to a particular core
      EventHeap* getEventHeapFromSimThreadId(SInt32 sim_thread_id);
      EventHeap* getEventHeapFromCoreId(core_id_t core_id);

      // Wake up the threads that are waiting on ready event heaps
      void wakeUpWaiters();
      // Checks if a particular event is ready
      inline bool isReady(UInt64 event_time);

      Event* createEvent(void* obj, UInt64 time, Event::Type event_type, \
            void* processing_entity, SInt32 event_heap_id);
      void destroyEvent(Event* event);

      // Static Functions to allocate and release memory at start of simulation
      void allocateEventMemory();
      void releaseEventMemory();

   private:
      // Different Thread Types
      enum TheadType
      {
         APP_THREAD = 0,
         SIM_THREAD,
         NUM_THREAD_TYPES
      };

      // Core Id to Event Heap Id mapping
      map<core_id_t, SInt32> _core_id_to_sim_thread_id_mapping;

      // The different event heaps
      MetaEventHeap* _global_meta_event_heap;
      MetaEventHeap* _app_meta_event_heap;
      MetaEventHeap* _sim_meta_event_heap;
      vector<EventHeap*> _sim_event_heap_list;

      // Event Memory Management
      SInt32 _max_num_outstanding_events_per_heap;
      SInt32 _num_event_heaps;
      // TODO: Cache line-align the memory later
      Event* _event_memory;
      // Variables that hold the locally managed memory
      vector<stack<Event*> > _free_memory_list;
      // Locks for the allocating and releasing memory
      vector<Lock> _event_memory_lock_list;
};

inline bool EventManager::isReady(UInt64 event_time)
{
   assert(event_time >= _global_meta_event_heap->getFirstEventTime());
   // TODO: Make this a range later
   return (event_time == _global_meta_event_heap->getFirstEventTime());
}
