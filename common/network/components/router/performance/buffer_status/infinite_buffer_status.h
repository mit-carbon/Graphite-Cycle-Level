#pragma once

#include <cassert>

#include "fixed_types.h"
#include "buffer_status.h"

class InfiniteBufferStatus : public BufferStatus
{
   public:
      InfiniteBufferStatus() : BufferStatus() {}
      ~InfiniteBufferStatus() {}

      void allocate(Flit* flit, SInt32 num_buffers) {}
      UInt64 tryAllocate(Flit* flit, SInt32 num_buffers) { return 0; }
      void receive(BufferManagementMsg* msg) { assert(false); }
};
