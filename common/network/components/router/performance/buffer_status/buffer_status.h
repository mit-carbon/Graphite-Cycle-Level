#pragma once

#include <cassert>

#include "buffer_management_scheme.h"
#include "flit.h"
#include "buffer_management_msg.h"

class BufferStatus
{
public:
   BufferStatus() {}
   virtual ~BufferStatus() {};

   virtual void allocate(Flit* flit, SInt32 num_buffers) = 0;
   virtual UInt64 tryAllocate(Flit* flit, SInt32 num_buffers) = 0;
   virtual void receive(BufferManagementMsg* msg) = 0;

   static BufferStatus* create(BufferManagementScheme::Type buffer_management_scheme, SInt32 size_buffer);
};
