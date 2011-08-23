#pragma once

#include "buffer_management_msg.h"
#include "finite_buffer_status.h"

class CreditStatus : public FiniteBufferStatus
{
   public:
      CreditStatus(SInt32 size_buffer);
      ~CreditStatus();

      void allocate(Flit* flit, SInt32 num_buffers);
      UInt64 tryAllocate(Flit* flit, SInt32 num_buffers);
      void receive(BufferManagementMsg* buffer_mangement_msg);
   
   private:
      SInt32 _credit_count;
};
