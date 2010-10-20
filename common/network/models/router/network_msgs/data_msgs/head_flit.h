#pragma once

#include "flit.h"

class HeadFlit : public Flit
{
   public:
      core_id_t _sender;
      core_id_t _receiver;
      UInt32 _length;
};
