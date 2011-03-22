#pragma once

#include <stdio.h>

#include "fixed_types.h"
#include "packetize.h"
#include "event_queue.h"

class Event
{
public:
   // Right now only network, but later add core, memory_system, etc
   enum Type
   {
      // Network Router -> Another Network Router
      NETWORK = 0,
      INSTRUCTION,
      // Core Model -> MMU
      INITIATE_MEMORY_ACCESS,
      COMPLETE_MEMORY_ACCESS,
      // MMU -> Cache
      INITIATE_CACHE_ACCESS,
      COMPLETE_CACHE_ACCESS,
      NUM_TYPES
   };

   Event(Type type, UInt64 time, UnstructuredBuffer& event_args)
      : _type(type), _time(time), _event_args(event_args) {}
   ~Event() {}

   static void processInOrder(Event* event, core_id_t recv_core_id, EventQueue::Type event_queue_type);

   virtual void process() = 0;

   Type getType() { return _type; }
   UInt64 getTime() { return _time; }

protected:
   Type _type;
   UInt64 _time;
   UnstructuredBuffer _event_args;
};

class EventNetwork : public Event
{
public:
   EventNetwork(UInt64 time, UnstructuredBuffer& event_args)
      : Event(NETWORK, time, event_args) {}
   ~EventNetwork() {}

   void process();
};

class EventInstruction : public Event
{
public:
   EventInstruction(UInt64 time, UnstructuredBuffer& event_args)
      : Event(INSTRUCTION, time, event_args) {}
   ~EventInstruction() {}

   void process();
};

class EventInitiateMemoryAccess : public Event
{
public:
   EventInitiateMemoryAccess(UInt64 time, UnstructuredBuffer& event_args)                       
      : Event(INITIATE_MEMORY_ACCESS, time, event_args) {}
   ~EventInitiateMemoryAccess() {}

   void process();
};

class EventCompleteMemoryAccess : public Event
{
public:
   EventCompleteMemoryAccess(UInt64 time, UnstructuredBuffer& event_args)
      : Event(COMPLETE_MEMORY_ACCESS, time, event_args) {}
   ~EventCompleteMemoryAccess() {}

   void process();
};

class EventInitiateCacheAccess : public Event
{
public:
   EventInitiateCacheAccess(UInt64 time, UnstructuredBuffer& event_args)
      : Event(INITIATE_CACHE_ACCESS, time, event_args) {}
   ~EventInitiateCacheAccess() {}

   void process();
};

class EventCompleteCacheAccess : public Event
{
public:
   EventCompleteCacheAccess(UInt64 time, UnstructuredBuffer& event_args)
      : Event(COMPLETE_CACHE_ACCESS, time, event_args) {}
   ~EventCompleteCacheAccess() {}

   void process();
};
