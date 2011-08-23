#pragma once

#include <vector>
using std::vector;

#include "flit.h"

class HeadFlit : public Flit
{
public:
   HeadFlit(SInt32 num_flits, SInt32 num_phits, core_id_t sender, core_id_t receiver)
      : Flit(Flit::HEAD, num_phits, sender, receiver)
      , _num_flits(num_flits)
      , _output_endpoint_list(NULL)
   {}
   HeadFlit(const HeadFlit& rhs)
      : Flit(rhs)
      , _num_flits(rhs._num_flits)
      , _output_endpoint_list(rhs._output_endpoint_list)
   {}
   ~HeadFlit() {}

   NetworkMsg* clone() { return new HeadFlit(*this); }
   UInt32 size() { return sizeof(*this); }

   // Data Fields
   SInt32 _num_flits;
   vector<Channel::Endpoint>* _output_endpoint_list;
};
