#include "buffer_model.h"
#include "infinite_buffer_model.h"
#include "finite_buffer_model_with_credit_signaling.h"
#include "finite_buffer_model_with_on_off_signaling.h"
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
   // Synchronize the flit time to the buffer time
   flit->_normalized_time = max<UInt64>(_queue_time, flit->_normalized_time);
}

void
BufferModel::updateBufferTime()
{
   Flit* flit = _queue.front();
   // Synchronize the buffer time to the flit time
   _queue_time = max<UInt64>(_queue_time, flit->_normalized_time + flit->_length);
}

BufferModel*
BufferModel::create(BufferManagementScheme::Type buffer_management_scheme, SInt32 buffer_size)
{
   switch (buffer_management_scheme)
   {
      case BufferManagementScheme::INFINITE:
         return new InfiniteBufferModel();

      case BufferManagementScheme::CREDIT:
         return new FiniteBufferModelWithCreditSignaling(buffer_size);

      case BufferManagementScheme::ON_OFF:
         return new FiniteBufferModelWithOnOffSignaling(buffer_size, 1 /* on_off_threshold */);

      default:
         LOG_PRINT_ERROR("Unrecognized Buffer Management Scheme(%u)", buffer_management_scheme);
         return (BufferModel*) NULL;
   }
}
