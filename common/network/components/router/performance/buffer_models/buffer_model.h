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
      queue<Flit*> _queue;
      UInt64 _queue_time;

   public:
      BufferModel();
      virtual ~BufferModel();

      static BufferModel* create(BufferManagementScheme::Type buffer_management_scheme, \
            SInt32 buffer_size);

      virtual BufferManagementMsg* enqueue(Flit* flit);
      virtual BufferManagementMsg* dequeue();
      Flit* front() { return _queue.front(); }
      bool empty() { return _queue.empty(); }
      size_t size() { return _queue.size(); }
     
      void updateFlitTime(); 
      void updateBufferTime();
      UInt64 getBufferTime();
};
