#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include "buffer_status_list.h"
#include "log.h"

BufferStatusList::BufferStatusList(SInt32 num_output_endpoints,
      BufferManagementScheme::Type buffer_management_scheme,
      SInt32 size_buffer)
   : _num_output_endpoints(num_output_endpoints)
   , _channel_free_time(0)
{
   _buffer_status_vec.resize(_num_output_endpoints);
   for (SInt32 i = 0; i < _num_output_endpoints; i++)
   {
      _buffer_status_vec[i] = BufferStatus::create(buffer_management_scheme, size_buffer);
   }
}

BufferStatusList::~BufferStatusList()
{
   for (SInt32 i = 0; i < _num_output_endpoints; i++)
   {
      delete _buffer_status_vec[i];
   }
}

void
BufferStatusList::allocateBuffer(Flit* flit, SInt32 endpoint_index, SInt32 num_buffers)
{
   // We can surely allocate buffers here
   if (endpoint_index == Channel::Endpoint::ALL)
   {
      // Broadcasted flit
      for (SInt32 i = 0; i < _num_output_endpoints; i++)
      {
         _buffer_status_vec[i]->allocate(flit, num_buffers);
      }
   }
   else
   {
      // Before allocating buffer, always update the buffer with the _channel_time
      _buffer_status_vec[endpoint_index]->allocate(flit, num_buffers);
   }

   // Update Channel Free Time
   _channel_free_time = flit->_normalized_time + num_buffers;
   LOG_PRINT("Updated Channel Free Time to %llu", _channel_free_time);
}

UInt64
BufferStatusList::tryAllocateBuffer(Flit* flit, SInt32 endpoint_index, SInt32 num_buffers)
{
   // Check if buffers can be allocated at the downstream router
   LOG_ASSERT_ERROR((endpoint_index == Channel::Endpoint::ALL) ||
                    ((endpoint_index >= 0) && (endpoint_index < _num_output_endpoints)),
                    "Invalid Endpoint Index(%i): should be ALL(0x%x) or within [0,%i]",
                    endpoint_index, Channel::Endpoint::ALL, _num_output_endpoints-1);

   // Initially, set the allocated time to time when channel is free
   UInt64 allocated_time = _channel_free_time;

   if (endpoint_index == Channel::Endpoint::ALL)
   {
      // Flit broadcasted to all endpoints of a channel
      for (SInt32 i = 0; (i < _num_output_endpoints) && (allocated_time != UINT64_MAX); i++)
      {
         allocated_time = max<UInt64>(allocated_time, _buffer_status_vec[i]->tryAllocate(flit, num_buffers));
      }
   }
   else
   {
      // Flit sent to only one endpoint of a channel
      allocated_time = max<UInt64>(allocated_time, _buffer_status_vec[endpoint_index]->tryAllocate(flit, num_buffers));
   }
  
   LOG_PRINT("Allocated Time(%llu)", allocated_time); 
   return allocated_time;
}

void
BufferStatusList::receiveBufferManagementMsg(BufferManagementMsg* buffer_msg, SInt32 endpoint_index)
{
   _buffer_status_vec[endpoint_index]->receive(buffer_msg);
}
