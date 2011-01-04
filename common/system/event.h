#pragma once

#include "fixed_types.h"

class Event
{
   public:
      // Right now only network, but later add core, memory_system, etc
      enum Type
      {
         NETWORK = 0,
         NUM_TYPES
      };

      Event();
      ~Event();

      void process();
      void init(void* obj, UInt64 time, Type type, void* processing_entity);

      UInt64 _time;

   private:
      // Private Variables
      void* _obj;
      Type _type;
      void* _processing_entity;
};
