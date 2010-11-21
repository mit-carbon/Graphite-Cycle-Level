#pragma once

#include "network_msg.h"
#include "buffer_management_scheme.h"

class BufferManagementMsg : public NetworkMsg
{
   public:
      BufferManagementMsg(BufferManagementScheme::Type type, UInt64 normalized_time):
         NetworkMsg(NetworkMsg::BUFFER_MANAGEMENT, normalized_time),
         _type(type)
      {}
      BufferManagementMsg(const BufferManagementMsg& rhs):
         NetworkMsg(rhs),
         _type(rhs._type)
      {}
      ~BufferManagementMsg() {}

      BufferManagementScheme::Type _type;
     
      NetworkMsg* clone() { return new BufferManagementMsg(*this); }
      UInt32 size() { return sizeof(*this); }
};
