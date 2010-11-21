#pragma once

#include <string>
using namespace std;

#include "link_model.h"

class ElectricalLinkModel : public LinkModel
{
   public:
      ElectricalLinkModel(volatile float link_frequency, volatile double link_length, \
            UInt32 link_width, SInt32 num_receiver_endpoints);
      ~ElectricalLinkModel();

      enum Type
      {
         REPEATED = 0,
         EQUALIZED,
         NUM_LINK_TYPES
      };

      static Type parse(string link_type_str);
};
