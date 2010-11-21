#pragma once

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

      NetworkMsg(Type type, UInt64 normalized_time = 0):
         _normalized_time(normalized_time),
         _sender_router_index(0),
         _receiver_router_index(0),
         _type(type),
         _input_endpoint(Channel::Endpoint()),
         _output_endpoint(Channel::Endpoint())
      {}

      NetworkMsg(const NetworkMsg& rhs):
         _normalized_time(rhs._normalized_time),
         _sender_router_index(rhs._sender_router_index),
         _receiver_router_index(rhs._receiver_router_index),
         _type(rhs._type),
         _input_endpoint(rhs._input_endpoint),
         _output_endpoint(rhs._output_endpoint)
      {}
      
      UInt64 _normalized_time;
      SInt32 _sender_router_index;
      SInt32 _receiver_router_index;
      Type _type;
      Channel::Endpoint _input_endpoint;
      Channel::Endpoint _output_endpoint;

      virtual NetworkMsg* clone() { return new NetworkMsg(*this); }
      virtual UInt32 size() { return sizeof(*this); }
};
