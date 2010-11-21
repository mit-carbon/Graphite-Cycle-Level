#pragma once

#include "buffer_management_msg.h"

class CreditMsg : public BufferManagementMsg
{
   public:
      CreditMsg(UInt64 normalized_time, SInt32 num_credits):
         BufferManagementMsg(BufferManagementScheme::CREDIT, normalized_time),
         _num_credits(num_credits)
      {}
      CreditMsg(const CreditMsg& rhs):
         BufferManagementMsg(rhs),
         _num_credits(rhs._num_credits)
      {}
      ~CreditMsg() {}

      SInt32 _num_credits;
      
      NetworkMsg* clone() { return new CreditMsg(*this); }
      UInt32 size() { return sizeof(*this); }
};
