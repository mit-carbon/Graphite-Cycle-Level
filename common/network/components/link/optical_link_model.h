#pragma once

#include "link_model.h"

class OpticalLinkModel : public LinkModel
{
   public:
      OpticalLinkModel(volatile float link_frequency, volatile double link_length, \
            UInt32 link_width, SInt32 num_receiver_endpoints):
         LinkModel(link_frequency, link_length, link_width, num_receiver_endpoints)
      {}
      ~OpticalLinkModel() {}
};
