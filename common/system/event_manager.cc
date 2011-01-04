#include "event_manager.h"
#include "config.h"

// TODO: Make this multi-process later
// 1) One EventManager per process
// 2) One _app_meta_event_heap & One _sim_meta_event_heap per process
// 3) One _global_meta_event_heap for the entire simulation
EventManager::EventManager()
{
   _global_meta_event_heap = new MetaEventHeap(NUM_THREAD_TYPES);
   _app_meta_event_heap = new MetaEventHeap(Config::getSingleton()->getApplicationCores(), \
         _global_meta_event_heap, APP_THREAD);
   _sim_meta_event_heap = new MetaEventHeap(Config::getSingleton()->getTotalSimThreads(), \
         _global_meta_event_heap, SIM_THREAD);

   for (UInt32 i = 0; i < Config::getSingleton()->getTotalSimThreads(); i++)
      _sim_event_heap_list.push_back(new EventHeap(i, _sim_meta_event_heap, i));

   // Allocate core ids' to sim thread ids' - Do a simple allocation now and refine later
   SInt32 sim_thread_id = 0;
   for (SInt32 i = 0; i < (SInt32) Config::getSingleton()->getTotalCores(); i++)
   {
      _core_id_to_sim_thread_id_mapping[i] = sim_thread_id;
      sim_thread_id = (sim_thread_id + 1) % Config::getSingleton()->getTotalSimThreads();
   }
   
   // Allocate Event Memory
   allocateEventMemory();
}

EventManager::~EventManager()
{
   // Release Event Memory
   releaseEventMemory();

   for (UInt32 i = 0; i < Config::getSingleton()->getTotalSimThreads(); i++)
      delete _sim_event_heap_list[i];
   
   delete _app_meta_event_heap;
   delete _sim_meta_event_heap;
   
   delete _global_meta_event_heap;
}

// Always returns the _sim_event_heap
EventHeap*
EventManager::getEventHeapFromSimThreadId(SInt32 sim_thread_id)
{
   assert(sim_thread_id < (SInt32) Config::getSingleton()->getTotalSimThreads());
   return _sim_event_heap_list[sim_thread_id];
}

EventHeap*
EventManager::getEventHeapFromCoreId(core_id_t core_id)
{
   SInt32 sim_thread_id = _core_id_to_sim_thread_id_mapping[core_id];
   return getEventHeapFromSimThreadId(sim_thread_id);
}

void
EventManager::wakeUpWaiters()
{
   // Wakes up only the sim threads for now
   // Once instruction and private cache modeling is made cycle-accurate,
   // wake up app threads also
   // Now, app threads run uncontrolled
   for (UInt32 i = 0; i < Config::getSingleton()->getTotalSimThreads(); i++)
   {
      if (isReady(_sim_event_heap_list[i]->getFirstEventTime()))
         _sim_event_heap_list[i]->signalEvent();
   }
}

Event*
EventManager::createEvent(void* obj, UInt64 time, Event::Type event_type, void* processing_entity, SInt32 event_heap_id)
{
   ScopedLock sl(_event_memory_lock_list[event_heap_id]);
   stack<Event*>& free_list = _free_memory_list[event_heap_id];

   Event* event = free_list.top();
   free_list.pop();

   event->init(obj, time, event_type, processing_entity);
   return event;
}

void
EventManager::destroyEvent(Event* event)
{
   SInt32 event_heap_id = ((SInt32) (event - _event_memory)) / _max_num_outstanding_events_per_heap;

   ScopedLock sl(_event_memory_lock_list[event_heap_id]);
   stack<Event*>& free_list = _free_memory_list[event_heap_id];

   free_list.push(event);
}

void
EventManager::allocateEventMemory()
{
   SInt32 max_num_outstanding_events = _max_num_outstanding_events_per_heap * _num_event_heaps;

   // TODO: Cache line align this later
   _event_memory = new Event[max_num_outstanding_events];
   _event_memory_lock_list.resize(_num_event_heaps);
   _free_memory_list.resize(_num_event_heaps);

   for (SInt32 i = 0; i < _num_event_heaps; i++)
   {
      for (SInt32 j = 0; j < _max_num_outstanding_events_per_heap; j++)
         _free_memory_list[i].push(&_event_memory[i * _max_num_outstanding_events_per_heap + j]);
   }
}

void
EventManager::releaseEventMemory()
{
   delete _event_memory;
}
