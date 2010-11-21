#include "list_of_buffer_usage_histories.h"

ListOfBufferUsageHistories::ListOfBufferUsageHistories(SInt32 num_output_endpoints, \
      BufferManagementScheme::Type buffer_management_scheme, \
      SInt32 size_buffer):
   _num_output_endpoints(num_output_endpoints),
   _channel_time(0)
{
   _buffer_usage_history_vec.resize(_num_output_endpoints);
   for (SInt32 i = 0; i < _num_output_endpoints; i++)
      _buffer_usage_history_vec[i] = BufferUsageHistory::create( \
            buffer_management_scheme, size_buffer);
}

ListOfBufferUsageHistories::~ListOfBufferUsageHistories()
{
   for (SInt32 i = 0; i < _num_output_endpoints; i++)
      delete _buffer_usage_history_vec[i];
}

bool
ListOfBufferUsageHistories::allocateBuffer(Flit* flit, SInt32 endpoint_index)
{
   if (endpoint_index == Channel::Endpoint::ALL)
   {
      // Broadcasted flit
      for (SInt32 i = 0; i < _num_output_endpoints; i++)
      {
         _buffer_usage_history_vec[i]->prune(_channel_time);
         bool allocated = _buffer_usage_history_vec[i]->allocate(flit);
         if (!allocated)
            return false;
      }
   }
   else
   {
      // Before allocating buffer, always update the buffer with the _channel_time
      _buffer_usage_history_vec[endpoint_index]->prune(_channel_time);
      bool allocated = _buffer_usage_history_vec[endpoint_index]->allocate(flit);
      if (!allocated)
         return false;
   }

   _channel_time = flit->_normalized_time + flit->_length;
   return true;
}

void
ListOfBufferUsageHistories::receiveBufferManagementMsg(BufferManagementMsg* buffer_msg, \
      SInt32 endpoint_index)
{
   _buffer_usage_history_vec[endpoint_index]->receive(buffer_msg);
}
