#pragma once

#include "fixed_types.h"
#include "buffer_usage_history.h"

class FiniteBufferUsageHistory : public BufferUsageHistory
{
   public:
      FiniteBufferUsageHistory(SInt32 size_buffer):
         BufferUsageHistory(), _size_buffer(size_buffer) {}
      ~FiniteBufferUsageHistory() {}

   protected:
      SInt32 _size_buffer;
};
