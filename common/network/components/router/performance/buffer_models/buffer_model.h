#pragma once

#include <queue>
using namespace std;

#include "fixed_types.h"
#include "flit.h"
#include "buffer_management_scheme.h"
#include "buffer_management_msg.h"

class BufferModel
{
   private:
      queue<Flit*> _buffer;
      UInt64 _buffer_time;

   public:
      BufferModel();
      virtual ~BufferModel();

      static BufferModel* create(BufferManagementScheme::Type buffer_management_scheme, \
            SInt32 buffer_size);

      virtual BufferManagementMsg* enqueue(Flit* flit);
      virtual BufferManagementMsg* dequeue();
      Flit* front() { return _buffer.front(); }
      bool empty() { return _buffer.empty(); }
      size_t size() { return _buffer.size(); }
     
      void updateFlitTime(); 
      void updateBufferTime();
      UInt64 getBufferTime();
};
