#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include "on_off_status.h"
#include "on_off_msg.h"
#include "log.h"

OnOffStatus::OnOffStatus(SInt32 size_buffer)
   : FiniteBufferStatus(size_buffer)
   , _on_off_status(true)
{}

OnOffStatus::~OnOffStatus()
{}

void
OnOffStatus::allocate(Flit* flit, SInt32 num_buffers)
{
   LOG_ASSERT_ERROR(_on_off_status, "On Off Status(FALSE)");
}

UInt64
OnOffStatus::tryAllocate(Flit* flit, SInt32 num_buffers)
{
   // Only possible with flit-buffer flow control
   LOG_ASSERT_ERROR(num_buffers == 1, "Num Buffers Requested must be 1 to work with On-Off Buffer Management Scheme");
   
   return (_on_off_status) ? _last_msg_time : UINT64_MAX;
}

void
OnOffStatus::receive(BufferManagementMsg* buffer_management_msg)
{
   FiniteBufferStatus::receive(buffer_management_msg);

   OnOffMsg* msg = dynamic_cast<OnOffMsg*>(buffer_management_msg);
   assert(msg->_on_off_status != _on_off_status);
   _on_off_status = msg->_on_off_status;
}
