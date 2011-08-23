#include "flit.h"
#include "log.h"

Flit::Flit(Type type, SInt32 num_phits, core_id_t sender, core_id_t receiver)
   : NetworkMsg(DATA)
   , _normalized_time_at_entry(0)
   , _num_phits(num_phits)
   , _type(type)
   , _sender(sender)
   , _receiver(receiver)
   , _zero_load_delay(0)
   , _net_packet(NULL)
{}

Flit::Flit(const Flit& rhs)
   : NetworkMsg(rhs)
   , _normalized_time_at_entry(rhs._normalized_time_at_entry)
   , _num_phits(rhs._num_phits)
   , _type(rhs._type)
   , _sender(rhs._sender)
   , _receiver(rhs._receiver)
   , _zero_load_delay(rhs._zero_load_delay)
   , _net_packet(rhs._net_packet)
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
