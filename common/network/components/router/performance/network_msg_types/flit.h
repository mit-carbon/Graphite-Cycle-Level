#pragma once

#include "network_msg.h"
#include "channel_endpoint_list.h"
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

      Flit(Type type, SInt32 length, core_id_t sender, core_id_t receiver, core_id_t requester):
         NetworkMsg(DATA),
         _normalized_time_at_entry(0),
         _length(length),
         _type(type),
         _sender(sender),
         _receiver(receiver),
         _requester(requester),
         _net_packet(NULL),
         _output_endpoint_list(NULL)
      {}

      Flit(const Flit& rhs):
         NetworkMsg(rhs),
         _normalized_time_at_entry(rhs._normalized_time_at_entry),
         _length(rhs._length),
         _type(rhs._type),
         _sender(rhs._sender),
         _receiver(rhs._receiver),
         _requester(rhs._requester),
         _net_packet(rhs._net_packet),
         _output_endpoint_list(rhs._output_endpoint_list)
      {}

      UInt64 _normalized_time_at_entry;
      SInt32 _length;
      Type _type;
      core_id_t _sender;
      core_id_t _receiver;
      core_id_t _requester;
      NetPacket* _net_packet;
      ChannelEndpointList* _output_endpoint_list;

      NetworkMsg* clone() { return new Flit(*this); }
      UInt32 size() { return sizeof(*this); }
};
