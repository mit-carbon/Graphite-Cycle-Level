#include "event.h"
#include "network.h"
#include "log.h"

Event::Event()
{}

Event::~Event()
{}

void
Event::init(void* obj, UInt64 time, Type type, void* processing_entity)
{
   _obj = obj;
   _time = time;
   _type = type;
   _processing_entity = processing_entity;
}

void
Event::process()
{
   switch(_type)
   {
      case NETWORK:
         ((Network*)_processing_entity)->processPacket((NetPacket*)_obj);
         break;

      default:
         LOG_PRINT_ERROR("Unrecognized Event Type(%u)", _type);
         break;
   }
}
