#pragma once

#include <cassert>

#include "fixed_types.h"
#include "buffer_usage_history.h"

class InfiniteBufferUsageHistory : public BufferUsageHistory
{
   public:
      InfiniteBufferUsageHistory();
      ~InfiniteBufferUsageHistory();

      bool allocate(Flit* flit);
      void prune(UInt64 time);
      void receive(BufferManagementMsg* msg) { assert(false); }

   private:
      UInt64 _channel_time;
};
