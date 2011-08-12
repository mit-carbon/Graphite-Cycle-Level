#pragma once

#include "fixed_types.h"
#include "finite_buffer_model.h"

class CreditBufferModel : public FiniteBufferModel
{
   public:
      CreditBufferModel(SInt32 size_buffer);
      ~CreditBufferModel();

      BufferManagementMsg* dequeue();
};
