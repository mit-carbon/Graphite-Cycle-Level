#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include "credit_status.h"
#include "credit_msg.h"
#include "utils.h"
#include "log.h"

CreditStatus::CreditStatus(SInt32 size_buffer)
   : FiniteBufferStatus(size_buffer)
   , _credit_count(size_buffer)
{}

CreditStatus::~CreditStatus()
{}

void
CreditStatus::allocate(Flit* flit, SInt32 num_buffers)
{
   LOG_ASSERT_ERROR(num_buffers <= _credit_count,
         "Num Buffers Requested(%i) > Credit Count(%i)", num_buffers, _credit_count);
   
   // Update Credits
   _credit_count -= num_buffers;
}

UInt64
CreditStatus::tryAllocate(Flit* flit, SInt32 num_buffers)
{
   return (num_buffers <= _credit_count) ? _last_msg_time : UINT64_MAX;
}

void
CreditStatus::receive(BufferManagementMsg* buffer_mangement_msg)
{
   FiniteBufferStatus::receive(buffer_mangement_msg);
   
   CreditMsg* credit_msg = dynamic_cast<CreditMsg*>(buffer_mangement_msg);
   _credit_count += credit_msg->_num_credits;
}
