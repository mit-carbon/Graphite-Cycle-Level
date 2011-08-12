#pragma once

#include <cassert>

#include "fixed_types.h"
#include "buffer_status.h"

class InfiniteBufferStatus : public BufferStatus
{
   public:
      InfiniteBufferStatus() : BufferStatus() {}
      ~InfiniteBufferStatus() {}

      void allocate(Flit* flit) {}
      UInt64 tryAllocate(Flit* flit) { return 0; }
      void receive(BufferManagementMsg* msg) { assert(false); }
};
