#pragma once

#include "network_msg.h"
#include "network.h"

class Flit : public NetworkMsg
{
   public:
      enum Type
      {
         HEAD = 0x001,
         BODY = 0x010,
         TAIL = 0x100,
         INVALID = -1
      };

      Flit(Type type, SInt32 length):
         NetworkMsg(DATA),
         _normalized_time_at_entry(0),
         _length(length),
         _type(type),
         _net_packet(NULL)
      {}

      Flit(const Flit& rhs):
         NetworkMsg(rhs),
         _normalized_time_at_entry(rhs._normalized_time_at_entry),
         _length(rhs._length),
         _type(rhs._type),
         _net_packet(rhs._net_packet)
      {}

      UInt64 _normalized_time_at_entry;
      SInt32 _length;
      Type _type;
      NetPacket* _net_packet;

      NetworkMsg* clone() { return new Flit(*this); }
      UInt32 size() { return sizeof(*this); }
};
