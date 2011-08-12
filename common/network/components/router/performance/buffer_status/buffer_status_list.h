#pragma once

#include <vector>
using namespace std;

#include "fixed_types.h"
#include "buffer_management_scheme.h"
#include "flit.h"
#include "buffer_management_msg.h"
#include "buffer_status.h"

// An object of this class is instantiated for every output channel
class BufferStatusList
{
   public:
      BufferStatusList(SInt32 num_output_endpoints, 
            BufferManagementScheme::Type buffer_management_scheme,
            SInt32 size_buffer);
      ~BufferStatusList();

      void allocateBuffer(Flit* flit, SInt32 endpoint_index);
      UInt64 tryAllocateBuffer(Flit* flit, SInt32 endpoint_index);
      void receiveBufferManagementMsg(BufferManagementMsg* buffer_msg, SInt32 endpoint_index);

   private:
      vector<BufferStatus*> _buffer_status_vec;
      SInt32 _num_output_endpoints;
      UInt64 _channel_free_time;
};
