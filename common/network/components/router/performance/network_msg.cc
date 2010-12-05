#include "network_msg.h"

NetworkMsg::NetworkMsg(Type type, UInt64 normalized_time):
   _normalized_time(normalized_time),
   _sender_router_index(0),
   _receiver_router_index(0),
   _type(type),
   _input_endpoint(Channel::Endpoint()),
   _output_endpoint(Channel::Endpoint())
{}

NetworkMsg::NetworkMsg(const NetworkMsg& rhs):
   _normalized_time(rhs._normalized_time),
   _sender_router_index(rhs._sender_router_index),
   _receiver_router_index(rhs._receiver_router_index),
   _type(rhs._type),
   _input_endpoint(rhs._input_endpoint),
   _output_endpoint(rhs._output_endpoint)
{}

NetworkMsg::~NetworkMsg()
{}

std::string
NetworkMsg::getTypeString()
{
   switch (_type)
   {
      case DATA:
         return "DATA";
      case BUFFER_MANAGEMENT:
         return "BUFFER_MANAGEMENT";
      default:
         return "";
   }

}
