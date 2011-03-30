#define __STDC_LIMIT_MACROS
#include <climits>
#include "simulator.h"
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
// 2) One _app_meta_event_heap & One _sim_meta_event_heap per process
// 3) One _global_meta_event_heap for the entire simulation
EventManager::EventManager()
{
   checkCycleAccurateMode();

   _global_meta_event_heap = new MetaEventHeap(NUM_THREAD_TYPES);
   _app_meta_event_heap = new MetaEventHeap(Config::getSingleton()->getApplicationCores(),
         _global_meta_event_heap, APP_THREAD);
   _sim_meta_event_heap = new MetaEventHeap(Config::getSingleton()->getTotalSimThreads(),
         _global_meta_event_heap, SIM_THREAD);

   // sim thread event queue managers
   for (UInt32 i = 0; i < Config::getSingleton()->getLocalSimThreadCount(); i++)
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
      EventHeap* event_heap = new EventHeap(event_queue_manager, _sim_meta_event_heap, i);
      UnorderedEventQueue* unordered_event_queue = new UnorderedEventQueue(event_queue_manager);
      event_queue_manager->setEventQueues(event_heap, unordered_event_queue);

      _event_queue_manager_list.push_back(event_queue_manager);
   }
}

EventManager::~EventManager()
{
   checkCycleAccurateMode();
   
   for (UInt32 i = 0; i < Config::getSingleton()->getTotalSimThreads(); i++)
   {
      delete _event_queue_manager_list[i]->getEventQueue(EventQueue::UNORDERED);
      delete _event_queue_manager_list[i]->getEventQueue(EventQueue::ORDERED);
      delete _event_queue_manager_list[i];
   }
   delete _app_meta_event_heap;
   delete _sim_meta_event_heap;
   delete _global_meta_event_heap;
}

EventQueueManager*
EventManager::getEventQueueManager(SInt32 sim_thread_id)
{
   checkCycleAccurateMode();
   
   assert(sim_thread_id < (SInt32) Config::getSingleton()->getTotalSimThreads());
   return _event_queue_manager_list[sim_thread_id];
}

bool
EventManager::isReady(UInt64 event_time)
{
   checkCycleAccurateMode();
   
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
   checkCycleAccurateMode();
   
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
   checkCycleAccurateMode();
   
   LOG_PRINT("processEventInOrder(%p): Processing CoreId(%i), EventQueueType(%s)",
         event, core_id, EventQueue::getName(event_queue_type).c_str());
   
   SInt32 sim_thread_id = Sim()->getSimThreadManager()->getSimThreadIDFromCoreID(core_id);
   // EventQueue is defined by (sim_thread_id, event_queue_type)
   EventQueueManager* event_queue_manager = getEventQueueManager(sim_thread_id);
   EventQueue* event_queue = event_queue_manager->getEventQueue(event_queue_type);
   event_queue->push(event);
}

void
EventManager::checkCycleAccurateMode()
{
   LOG_ASSERT_ERROR(Config::getSingleton()->getSimulationMode() == Config::CYCLE_ACCURATE,
         "EventManager() functions only called in cycle_accurate mode, curr mode(%u)",
         Config::getSingleton()->getSimulationMode());
}
