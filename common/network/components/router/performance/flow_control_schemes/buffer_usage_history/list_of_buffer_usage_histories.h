#pragma once

#include <vector>
using namespace std;

#include "fixed_types.h"
#include "buffer_management_scheme.h"
#include "flit.h"
#include "buffer_management_msg.h"
#include "buffer_usage_history.h"

class ListOfBufferUsageHistories
{
   public:
      ListOfBufferUsageHistories(SInt32 num_output_endpoints, \
            BufferManagementScheme::Type buffer_management_scheme, \
            SInt32 size_buffer);
      ~ListOfBufferUsageHistories();

      bool allocateBuffer(Flit* flit, SInt32 endpoint_index);
      void receiveBufferManagementMsg(BufferManagementMsg* buffer_msg, SInt32 endpoint_index);

   private:
      vector<BufferUsageHistory*> _buffer_usage_history_vec;
      SInt32 _num_output_endpoints;
      UInt64 _channel_time;
};
