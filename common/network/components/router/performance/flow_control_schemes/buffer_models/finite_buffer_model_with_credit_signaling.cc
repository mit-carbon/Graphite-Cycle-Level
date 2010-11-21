#include "finite_buffer_model_with_credit_signaling.h"
#include "credit_msg.h"

FiniteBufferModelWithCreditSignaling::FiniteBufferModelWithCreditSignaling(SInt32 size_buffer):
   FiniteBufferModel(size_buffer)
{}

FiniteBufferModelWithCreditSignaling::~FiniteBufferModelWithCreditSignaling()
{}

BufferManagementMsg*
FiniteBufferModelWithCreditSignaling::dequeue()
{
   Flit* flit = BufferModel::front();
   CreditMsg* credit_msg = new CreditMsg(flit->_normalized_time, flit->_length);
   
   BufferModel::dequeue();
   
   return credit_msg;
}
