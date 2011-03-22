#pragma once

#include <cassert>
#include <map>
#include <vector>
using std::map;
using std::vector;

#include "fixed_types.h"
#include "meta_event_heap.h"
#include "event.h"
#include "event_queue.h"

class EventQueueManager;

class EventManager
{
   public:
      EventManager();
      ~EventManager();

      // Get the EventQueue corresponding to a particular core
      EventQueueManager* getEventQueueManager(SInt32 sim_thread_id);

      // Wake up the threads that are waiting on ready event heaps
      void wakeUpWaiters();
      // Checks if a particular event is ready
      bool isReady(UInt64 event_time);

      // Create an event and push it onto the processing sim thread's queue
      void processEventInOrder(Event* event, core_id_t core_id, EventQueue::Type event_queue_type);

   private:
      // Different Thread Types
      enum TheadType
      {
         APP_THREAD = 0,
         SIM_THREAD,
         NUM_THREAD_TYPES
      };

      // The different event heaps - Works only with single process
      MetaEventHeap* _global_meta_event_heap;
      MetaEventHeap* _app_meta_event_heap;
      MetaEventHeap* _sim_meta_event_heap;
      vector<EventQueueManager*> _event_queue_manager_list;
};
