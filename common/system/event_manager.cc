#define __STDC_LIMIT_MACROS
#include <climits>
#include "simulator.h"
#include "core_manager.h"
#include "core.h"
#include "performance_model.h"
#include "sim_thread_manager.h"
#include "event_manager.h"
#include "event_queue_manager.h"
#include "event_heap.h"
#include "unordered_event_queue.h"
#include "event_queue.h"
#include "meta_event_heap.h"
#include "event.h"
#include "config.h"
#include "log.h"

// 1) One EventManager per process
// 2) One _global_meta_event_heap for the entire simulation
EventManager::EventManager()
{
   _global_meta_event_heap = new MetaEventHeap(Config::getSingleton()->getTotalSimThreads());

   // sim thread event queue managers
   for (UInt32 i = 0; i < Config::getSingleton()->getTotalSimThreads(); i++)
   {
      // There is one EventQueueManager per sim thread.
      // The EventQueueManager manages the
      //    1) OrderedEventQueue (aka EventHeap), and the
      //    2) UnorderedEventQueue
      // belonging to a particular sim thread
      // 
      // 1) The events on the EventHeap are processed in the order of simulated timestamps
      // 2) The events on the UnorderedEventQueue are processed as and when they come
      //                (in the order of real time)
      EventQueueManager* event_queue_manager = new EventQueueManager(i);
      EventHeap* event_heap = new EventHeap(event_queue_manager, _global_meta_event_heap, i);
      UnorderedEventQueue* unordered_event_queue = new UnorderedEventQueue(event_queue_manager);
      event_queue_manager->setEventQueues(event_heap, unordered_event_queue);

      _event_queue_manager_list.push_back(event_queue_manager);
   }
}

EventManager::~EventManager()
{
   for (UInt32 i = 0; i < Config::getSingleton()->getTotalSimThreads(); i++)
   {
      delete _event_queue_manager_list[i]->getEventQueue(EventQueue::UNORDERED);
      delete _event_queue_manager_list[i]->getEventQueue(EventQueue::ORDERED);
      delete _event_queue_manager_list[i];
   }
   delete _global_meta_event_heap;
}

EventQueueManager*
EventManager::getEventQueueManager(SInt32 sim_thread_id)
{
   assert(sim_thread_id < (SInt32) Config::getSingleton()->getTotalSimThreads());
   return _event_queue_manager_list[sim_thread_id];
}

bool
EventManager::isReady(UInt64 event_time)
{
   UInt64 global_time = _global_meta_event_heap->getFirstEventTime();
   LOG_ASSERT_ERROR(event_time >= global_time,
         "Event Time(%llu), Global Time(%llu)", 
         event_time, global_time);

   // TODO: Make this a range later
   return ( (event_time == _global_meta_event_heap->getFirstEventTime()) && (event_time != UINT64_MAX) );
}

void
EventManager::wakeUpWaiters()
{
   LOG_PRINT("wakeUpWaiters()");
   
   // Wakes up only the sim threads for now
   // Once instruction and private cache modeling is made cycle-accurate,
   // wake up app threads also. Now, app threads run uncontrolled
   for (UInt32 i = 0; i < Config::getSingleton()->getTotalSimThreads(); i++)
      _event_queue_manager_list[i]->signalEvent();
}

void
EventManager::processEventInOrder(Event* event, core_id_t core_id, EventQueue::Type event_queue_type)
{
   LOG_PRINT("Queueing Event (Event[%p], Processing CoreId[%i], EventQueueType[%s]",
         event, core_id, EventQueue::getName(event_queue_type).c_str());
   
   SInt32 sim_thread_id = Sim()->getSimThreadManager()->getSimThreadIDFromCoreID(core_id);
   // EventQueue is defined by (sim_thread_id, event_queue_type)
   EventQueueManager* event_queue_manager = getEventQueueManager(sim_thread_id);
   EventQueue* event_queue = event_queue_manager->getEventQueue(event_queue_type);
   
   if (event->getType() == Event::START_THREAD)
   {
      LOG_PRINT("Queueing Event (START_THREAD)");

      // Acquire LocalHeap and GlobalHeap locks
      assert(event_queue_type == EventQueue::ORDERED);
      event_queue->acquireLock();
      _global_meta_event_heap->acquireLock();
      
      // Get the current global time and set that as the event time
      EventStartThread* event_start_thread = (EventStartThread*) event;
      UInt64 global_time = _global_meta_event_heap->getFirstEventTime();
      if (global_time != UINT64_MAX)
         event_start_thread->setTime(global_time);

      // Set the Time in the Performance Model
      Core* core = Sim()->getCoreManager()->getCoreFromID(core_id);
      assert(core);
      core->getPerformanceModel()->setTime(event_start_thread->getTime());
   
      // Push the event by using the pre-locked version of the function
      event_queue->push(event_start_thread, true /* is_locked */ );

      // Release the Locks on the LocalHeap and GlobalHeap
      _global_meta_event_heap->releaseLock();
      event_queue->releaseLock();
   }
   else
   {
      event_queue->push(event);
   }
}
