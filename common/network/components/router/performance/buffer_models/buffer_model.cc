#include "buffer_model.h"
#include "infinite_buffer_model.h"
#include "credit_buffer_model.h"
#include "on_off_buffer_model.h"
#include "log.h"

BufferModel::BufferModel()
{
   _queue_time = 0;
}

BufferModel::~BufferModel()
{
   assert(_queue.empty());
}

BufferManagementMsg*
BufferModel::enqueue(Flit* flit)
{
   _queue.push(flit);
   return NULL;
}

BufferManagementMsg*
BufferModel::dequeue()
{
   _queue.pop();
   return NULL;
}

void
BufferModel::updateFlitTime()
{
   Flit* flit = _queue.front();
   
   LOG_PRINT("updateFlitTime() enter: Flit Time(%llu), Buffer Time(%llu)",
         flit->_normalized_time, _queue_time);

   // Synchronize the flit time to the buffer time
   flit->_normalized_time = max<UInt64>(_queue_time, flit->_normalized_time);
   
   LOG_PRINT("updateFlitTime() exit: Flit Time(%llu)", flit->_normalized_time);
}

void
BufferModel::updateBufferTime()
{
   Flit* flit = _queue.front();
   
   LOG_PRINT("updateBufferTime() enter: Flit Time(%llu), Flit Length(%i), Buffer Time(%llu)",
         flit->_normalized_time, flit->_length, _queue_time);

   LOG_ASSERT_ERROR(flit->_normalized_time >= _queue_time,
         "Flit Time(%llu) < Queue Time(%llu)", flit->_normalized_time, _queue_time);
   _queue_time = flit->_normalized_time + flit->_length;
   
   LOG_PRINT("updateBufferTime() exit: Buffer Time(%llu)", _queue_time);
}

UInt64
BufferModel::getBufferTime()
{
   LOG_PRINT("getBufferTime(): Buffer Time(%llu)", _queue_time);
   return _queue_time;
}

BufferModel*
BufferModel::create(BufferManagementScheme::Type buffer_management_scheme, SInt32 buffer_size)
{
   switch (buffer_management_scheme)
   {
      case BufferManagementScheme::INFINITE:
         return new InfiniteBufferModel();

      case BufferManagementScheme::CREDIT:
         return new CreditBufferModel(buffer_size);

      case BufferManagementScheme::ON_OFF:
         return new OnOffBufferModel(buffer_size, 4 /* on_off_threshold */);

      default:
         LOG_PRINT_ERROR("Unrecognized Buffer Management Scheme(%u)", buffer_management_scheme);
         return (BufferModel*) NULL;
   }
}
