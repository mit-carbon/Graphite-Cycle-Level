#pragma once

#include <string>

#include "fixed_types.h"
#include "channel.h"

class NetworkMsg
{
   public:
      enum Type
      {
         DATA = 0,
         BUFFER_MANAGEMENT,
         NUM_NETWORK_MSG_TYPES
      };

      NetworkMsg(Type type, UInt64 normalized_time = 0);
      NetworkMsg(const NetworkMsg& rhs);
      ~NetworkMsg();
      
      UInt64 _normalized_time;
      SInt32 _sender_router_index;
      SInt32 _receiver_router_index;
      Type _type;
      Channel::Endpoint _input_endpoint;
      Channel::Endpoint _output_endpoint;

      virtual NetworkMsg* clone() { return new NetworkMsg(*this); }
      virtual UInt32 size() { return sizeof(*this); }
      std::string getTypeString();
};
