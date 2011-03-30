#include "infinite_buffer_usage_history.h"
#include "utils.h"

InfiniteBufferUsageHistory::InfiniteBufferUsageHistory():
   BufferUsageHistory()
{}

InfiniteBufferUsageHistory::~InfiniteBufferUsageHistory()
{}

bool
InfiniteBufferUsageHistory::allocate(Flit* flit)
{
   UInt64 max_time = getMax<UInt64>(_channel_time, flit->_normalized_time);
   flit->_normalized_time = max_time;
   _channel_time = flit->_normalized_time + flit->_length;
   return true;
}

void
InfiniteBufferUsageHistory::prune(UInt64 time)
{
   _channel_time = time;
}
