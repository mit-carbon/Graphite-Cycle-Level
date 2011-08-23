#pragma once

#include <list>
using namespace std;

#include "on_off_msg.h"
#include "finite_buffer_status.h"

class OnOffStatus : public FiniteBufferStatus
{
   public:
      OnOffStatus(SInt32 size_buffer);
      ~OnOffStatus();

      void allocate(Flit* flit, SInt32 num_buffers);
      UInt64 tryAllocate(Flit* flit, SInt32 num_buffers);
      void receive(BufferManagementMsg* buffer_mangement_msg);
   
   private:
      bool _on_off_status;
};
