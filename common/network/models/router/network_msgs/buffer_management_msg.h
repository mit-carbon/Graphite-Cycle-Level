#pragma once

#include "network_msg.h"

class BufferManagementMsg : public NetworkMsg
{
   public:
      enum Type
      {
         INVALID = 0,
         MIN_MSG_TYPE,
         CREDIT = MIN_MSG_TYPE,
         ON_OFF,
         ACK_NACK,
         MAX_MSG_TYPE = ACK_NACK,
         NUM_MSG_TYPES = MAX_MSG_TYPE - MIN_MSG_TYPE + 1
      };

      Type _type;
      UInt64 _time;
};
