#pragma once

#include <string>

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

      // Public Functions
      Flit(Type type, SInt32 length, core_id_t sender, core_id_t receiver, core_id_t requester);
      Flit(const Flit& rhs);
      ~Flit();
       
      NetworkMsg* clone() { return new Flit(*this); }
      UInt32 size() { return sizeof(*this); }
      std::string getTypeString();
      
      // Data Fields
      UInt64 _normalized_time_at_entry;
      SInt32 _length;
      Type _type;
      core_id_t _sender;
      core_id_t _receiver;
      core_id_t _requester;
      NetPacket* _net_packet;
      ChannelEndpointList* _output_endpoint_list;
};
