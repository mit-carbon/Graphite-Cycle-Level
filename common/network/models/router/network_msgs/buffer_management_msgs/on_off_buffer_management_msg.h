#pragma once

#include "buffer_management_msg.h"

class OnOffBufferManagementMsg : public BufferManagementMsg
{
   public:
      bool _on_off_status; // true for 'on', false for 'off'
};
