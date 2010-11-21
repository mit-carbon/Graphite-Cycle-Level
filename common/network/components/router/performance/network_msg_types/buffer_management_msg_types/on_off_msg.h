#pragma once

#include "buffer_management_msg.h"

class OnOffMsg : public BufferManagementMsg
{
   public:
      OnOffMsg(UInt64 normalized_time, bool on_off_status):
         BufferManagementMsg(BufferManagementScheme::ON_OFF, normalized_time),
         _on_off_status(on_off_status)
      {}
      OnOffMsg(const OnOffMsg& rhs):
         BufferManagementMsg(rhs),
         _on_off_status(rhs._on_off_status)
      {}
      ~OnOffMsg() {}

      bool _on_off_status; // true for 'on', false for 'off'

      NetworkMsg* clone() { return new OnOffMsg(*this); }
      UInt32 size() { return sizeof(*this); }
};
