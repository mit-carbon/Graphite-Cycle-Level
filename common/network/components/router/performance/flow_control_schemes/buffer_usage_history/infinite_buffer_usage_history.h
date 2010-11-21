#pragma once

#include <cassert>

#include "fixed_types.h"
#include "buffer_usage_history.h"

class InfiniteBufferUsageHistory : public BufferUsageHistory
{
   public:
      InfiniteBufferUsageHistory(): BufferUsageHistory() {}
      ~InfiniteBufferUsageHistory() {}

      bool allocate(Flit* flit) { return true; }
      void prune(UInt64 time) { }
      void receive(BufferManagementMsg* msg) { assert(false); }
};
