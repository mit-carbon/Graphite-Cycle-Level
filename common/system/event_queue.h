#pragma once

#include <string>
#include "fixed_types.h"

class EventQueueManager;
class Event;

class EventQueue
{
   public:
      enum Type
      {
         ORDERED = 0,
         UNORDERED,
         NUM_TYPES
      };

      EventQueue(EventQueueManager* event_queue_manager):
         _event_queue_manager(event_queue_manager) {}
      virtual ~EventQueue() {}

      virtual void processEvents() = 0;
      virtual void push(Event* event, bool is_locked = false) = 0;
      virtual void acquireLock() = 0;
      virtual void releaseLock() = 0;

      static std::string getName(Type type)
      { return (type == ORDERED) ? "ORDERED" : "UNORDERED"; }

   protected:
      EventQueueManager* getEventQueueManager()
      { return _event_queue_manager; }

   private:
      EventQueueManager* _event_queue_manager;
};
