#pragma once

#include <string>
#include "network_msg.h"
#include "buffer_management_scheme.h"

class BufferManagementMsg : public NetworkMsg
{
   public:
      BufferManagementMsg(BufferManagementScheme::Type type, UInt64 normalized_time):
         NetworkMsg(NetworkMsg::BUFFER_MANAGEMENT, normalized_time),
         _type(type),
         _average_rate_of_progress(0.0)
      {}
      BufferManagementMsg(const BufferManagementMsg& rhs):
         NetworkMsg(rhs),
         _type(rhs._type),
         _average_rate_of_progress(rhs._average_rate_of_progress)
      {}
      ~BufferManagementMsg() {}

      BufferManagementScheme::Type _type;
      volatile double _average_rate_of_progress;
     
      NetworkMsg* clone() { return new BufferManagementMsg(*this); }
      UInt32 size() { return sizeof(*this); }
      std::string getTypeString() { return BufferManagementScheme::getTypeString(_type); }
};
