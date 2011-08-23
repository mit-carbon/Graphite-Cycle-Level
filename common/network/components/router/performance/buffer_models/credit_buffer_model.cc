#include "credit_buffer_model.h"
#include "credit_msg.h"
#include "log.h"

CreditBufferModel::CreditBufferModel(SInt32 size_buffer)
   : FiniteBufferModel(size_buffer)
{}

CreditBufferModel::~CreditBufferModel()
{}

BufferManagementMsg*
CreditBufferModel::dequeue()
{
   Flit* flit = BufferModel::front();
   CreditMsg* credit_msg = new CreditMsg(flit->_normalized_time, flit->_num_phits);

   LOG_PRINT("Data: allocate(%p)", credit_msg);

   BufferModel::dequeue();
   
   return credit_msg;
}
