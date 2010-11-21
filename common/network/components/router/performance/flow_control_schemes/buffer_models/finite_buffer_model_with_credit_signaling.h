#pragma once

#include "fixed_types.h"
#include "finite_buffer_model.h"

class FiniteBufferModelWithCreditSignaling : public FiniteBufferModel
{
   public:
      FiniteBufferModelWithCreditSignaling(SInt32 size_buffer);
      ~FiniteBufferModelWithCreditSignaling();

      BufferManagementMsg* dequeue();
};
