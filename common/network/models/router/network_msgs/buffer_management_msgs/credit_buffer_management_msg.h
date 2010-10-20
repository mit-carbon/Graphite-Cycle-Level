#pragma once

#include "buffer_management_msg.h"

class CreditBufferManagementMsg : public BufferManagementMsg
{
   public:
      UInt32 _num_credits;
};
