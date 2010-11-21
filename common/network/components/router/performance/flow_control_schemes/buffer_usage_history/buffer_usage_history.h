#pragma once

#include "buffer_management_scheme.h"
#include "flit.h"
#include "buffer_management_msg.h"

class BufferUsageHistory
{
   public:
      BufferUsageHistory() {}
      virtual ~BufferUsageHistory() = 0;

      virtual bool allocate(Flit* flit) = 0;
      virtual void prune(UInt64 time) = 0;
      virtual void receive(BufferManagementMsg* msg) = 0;

      static BufferUsageHistory* create(BufferManagementScheme::Type buffer_management_scheme, \
            SInt32 size_buffer);
};
