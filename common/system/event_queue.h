#pragma once

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
      virtual void push(Event* event) = 0;

   protected:
      EventQueueManager* getEventQueueManager()
      { return _event_queue_manager; }

   private:
      EventQueueManager* _event_queue_manager;
};
