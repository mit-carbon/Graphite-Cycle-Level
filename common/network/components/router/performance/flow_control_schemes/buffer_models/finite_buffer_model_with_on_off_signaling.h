#pragma once

#include "fixed_types.h"
#include "finite_buffer_model.h"

class FiniteBufferModelWithOnOffSignaling : public FiniteBufferModel
{
   public:
      FiniteBufferModelWithOnOffSignaling(SInt32 size_buffer, SInt32 on_off_threshold);
      ~FiniteBufferModelWithOnOffSignaling();

      BufferManagementMsg* enqueue(Flit* flit);
      BufferManagementMsg* dequeue();
   
   private:
      class BufferOccupancyStatus
      {
         public:
            BufferOccupancyStatus():
               _count(0), _time(0) {}
            ~BufferOccupancyStatus() {}

            SInt32 _count;
            UInt64 _time;
      };

      SInt32 _on_off_threshold;
      BufferOccupancyStatus _buffer_occupancy_status;
};
