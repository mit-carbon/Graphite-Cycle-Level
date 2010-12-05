#include "flit.h"
#include "log.h"

Flit::Flit(Type type, SInt32 length, core_id_t sender, core_id_t receiver, core_id_t requester):
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

Flit::Flit(const Flit& rhs):
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

Flit::~Flit()
{}

std::string
Flit::getTypeString()
{
   if (_type & HEAD)
      return "HEAD";
   else if (_type & TAIL)
      return "TAIL";
   else if (_type & BODY)
      return "BODY";
   else
      return "";
}
