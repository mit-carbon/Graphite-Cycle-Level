#pragma once

#include "flit.h"
#include "channel_endpoint_list.h"

class HeadFlit : public Flit
{
   public:
      HeadFlit(SInt32 length, core_id_t sender, core_id_t receiver):
         Flit(Flit::HEAD, length),
         _sender(sender),
         _receiver(receiver)
      {}

      HeadFlit(const HeadFlit& rhs):
         Flit(rhs),
         _sender(rhs._sender),
         _receiver(rhs._receiver),
         _output_endpoint_list(rhs._output_endpoint_list)
      {}

      core_id_t _sender;
      core_id_t _receiver;
      ChannelEndpointList* _output_endpoint_list;
      
      NetworkMsg* clone() { return new HeadFlit(*this); }
      UInt32 size() { return sizeof(*this); }
};
