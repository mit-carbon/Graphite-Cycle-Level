#pragma once

#include "fixed_types.h"

class LinkModel
{
   public:
      LinkModel(volatile float link_frequency, volatile double link_length, \
            UInt32 link_width, SInt32 num_receiver_endpoints):
         _link_frequency(link_frequency),
         _link_length(link_length),
         _link_width(link_width),
         _num_receiver_endpoints(num_receiver_endpoints)
      {}
      virtual ~LinkModel() {}

   protected:
      volatile float _link_frequency;
      volatile double _link_length;
      UInt32 _link_width;
      SInt32 _num_receiver_endpoints;
};
