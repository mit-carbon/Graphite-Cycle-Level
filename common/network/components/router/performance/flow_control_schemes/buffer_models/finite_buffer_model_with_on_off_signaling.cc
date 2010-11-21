#include "finite_buffer_model_with_on_off_signaling.h"
#include "on_off_msg.h"

FiniteBufferModelWithOnOffSignaling::FiniteBufferModelWithOnOffSignaling( \
      SInt32 size_buffer, SInt32 on_off_threshold):
   FiniteBufferModel(size_buffer),
   _on_off_threshold(on_off_threshold)
{}

FiniteBufferModelWithOnOffSignaling::~FiniteBufferModelWithOnOffSignaling()
{}

BufferManagementMsg*
FiniteBufferModelWithOnOffSignaling::enqueue(Flit* flit)
{
   BufferModel::enqueue(flit);

   UInt64 time = flit->_normalized_time;
   assert(flit->_length == 1);
   OnOffMsg* on_off_msg = (OnOffMsg*) NULL;

   // Synchronize 'time' to that of the _buffer_occupancy_status
   // +1 to simplify processing at the upstream router
   UInt64 delay = (_buffer_occupancy_status._time > time) ? \
                  (_buffer_occupancy_status._time - time) : 0;
   time += delay;

   if ((_size_buffer - _buffer_occupancy_status._count) == _on_off_threshold)
   {
      on_off_msg = new OnOffMsg(time, false);
   }
   _buffer_occupancy_status._count ++;
   
   // Update empty flit status time
   _buffer_occupancy_status._time = time;

   return on_off_msg;
}

BufferManagementMsg*
FiniteBufferModelWithOnOffSignaling::dequeue()
{
   Flit* flit = BufferModel::front();

   UInt64 time = flit->_normalized_time;
   assert(flit->_length == 1);
   OnOffMsg* on_off_msg = (OnOffMsg*) NULL;

   // Synchronize 'time' to that of the _empty_flit_status
   // +1 to simplify processing at the upstream router
   UInt64 delay = (_buffer_occupancy_status._time > time) ? \
                  (_buffer_occupancy_status._time - time) : 0;
   time += delay;

   _buffer_occupancy_status._count --;
   if ((_size_buffer - _buffer_occupancy_status._count) == _on_off_threshold)
   {
      on_off_msg = new OnOffMsg(time, true);
   }
   
   // Update empty flit status time
   _buffer_occupancy_status._time = time;

   BufferModel::dequeue();
   
   return on_off_msg;
}
