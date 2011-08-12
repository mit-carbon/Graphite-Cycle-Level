#pragma once

#include "fixed_types.h"
#include "buffer_status.h"

class FiniteBufferStatus : public BufferStatus
{
public:
   FiniteBufferStatus(SInt32 size_buffer)
      : BufferStatus()
      , _size_buffer(size_buffer)
      , _last_msg_time(0)
   {}
   ~FiniteBufferStatus() {}

   void receive(BufferManagementMsg* msg)
   {
      assert(msg->_normalized_time > _last_msg_time);
      _last_msg_time = msg->_normalized_time;
   }

protected:
   SInt32 _size_buffer;
   UInt64 _last_msg_time;
};
