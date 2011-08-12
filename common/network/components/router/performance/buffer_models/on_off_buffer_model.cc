#include "on_off_buffer_model.h"
#include "on_off_msg.h"
#include "log.h"

OnOffBufferModel::OnOffBufferModel(
      SInt32 size_buffer, SInt32 on_off_threshold)
   : FiniteBufferModel(size_buffer)
   , _on_off_threshold(on_off_threshold)
   , _free_buffer_count(size_buffer)
{}

OnOffBufferModel::~OnOffBufferModel()
{}

BufferManagementMsg*
OnOffBufferModel::enqueue(Flit* flit)
{
   LOG_ASSERT_ERROR(flit->_length == 1, "On Off Buffer Management Scheme only works with Flit Buffer Flow Control Schemes");
   
   // Enqueue the flit to buffer
   BufferModel::enqueue(flit);

   OnOffMsg* on_off_msg = (OnOffMsg*) NULL;
   
   // Decrease Free Buffer Count
   _free_buffer_count -= 1;   // Flit Length is 1
   if (_free_buffer_count == _on_off_threshold)
   {
      // Send an Off Msg to upstream router
      on_off_msg = new OnOffMsg(flit->_normalized_time, false);
   }

   return on_off_msg;
}

BufferManagementMsg*
OnOffBufferModel::dequeue()
{
   // Get the flit at the front of buffer
   Flit* flit = BufferModel::front();

   LOG_ASSERT_ERROR(flit->_length == 1, "On Off Buffer Management Scheme only works with Flit Buffer Flow Control Schemes");
   
   OnOffMsg* on_off_msg = (OnOffMsg*) NULL;
   
   if (_free_buffer_count == _on_off_threshold)
   {
      // Send an On Msg to upstream router
      on_off_msg = new OnOffMsg(flit->_normalized_time, true);
   }
   // Increase Free Buffer Count
   _free_buffer_count += 1;   // Flit Length is 1

   // Dequeue the flit from buffer
   BufferModel::dequeue();
   
   return on_off_msg;
}
