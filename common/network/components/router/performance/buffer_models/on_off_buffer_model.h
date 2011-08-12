#pragma once

#include "fixed_types.h"
#include "finite_buffer_model.h"

class OnOffBufferModel : public FiniteBufferModel
{
   public:
      OnOffBufferModel(SInt32 size_buffer, SInt32 on_off_threshold);
      ~OnOffBufferModel();

      BufferManagementMsg* enqueue(Flit* flit);
      BufferManagementMsg* dequeue();
   
   private:
      SInt32 _on_off_threshold;
      SInt32 _free_buffer_count;
};
