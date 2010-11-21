#pragma once

#include "fixed_types.h"
#include "buffer_model.h"

class FiniteBufferModel : public BufferModel
{
   public:
      FiniteBufferModel(SInt32 size_buffer): 
         BufferModel(), _size_buffer(size_buffer) {}
      ~FiniteBufferModel() {}
   
   protected:
      SInt32 _size_buffer;
};
