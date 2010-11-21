#include "credit_history.h"

CreditHistory::CreditHistory(SInt32 size_buffer):
   FiniteBufferUsageHistory(size_buffer)
{
   _credit_history.push_back(new CreditMsg(0, _size_buffer));
}

CreditHistory::~CreditHistory()
{}

bool
CreditHistory::allocate(Flit* flit)
{
   assert(!_credit_history.empty());
   list<CreditMsg*>::iterator it = _credit_history.begin();
   for ( ; it != _credit_history.end(); it++)
   {
      if ((*it)->_num_credits >= flit->_length)
      {
         UInt64 buffer_allocation_delay = ((*it)->_normalized_time > flit->_normalized_time) ? \
                                          ((*it)->_normalized_time - flit->_normalized_time) : 0;
         flit->_normalized_time += buffer_allocation_delay;

         // Update Credit History
         decreaseCredits(flit->_normalized_time + flit->_length, flit->_length);
         return true;
      }
   }
   return false;
}

void
CreditHistory::decreaseCredits(UInt64 time, SInt32 num_credits)
{
   assert(!_credit_history.empty());
   
   list<CreditMsg*>::iterator it = _credit_history.begin();
   list<CreditMsg*>::iterator prev_it;
   for ( ; it != _credit_history.end(); it++)
   {
      if ((*it)->_normalized_time > time)
         break;
      prev_it = it;
   }

   assert(it != _credit_history.begin());
   SInt32 last_credits = (*prev_it)->_num_credits;
   assert(last_credits >= num_credits);

   // Erase the credit history till then
   for (list<CreditMsg*>::iterator it2 = _credit_history.begin(); it2 != it; it2 ++)
   {
      delete (*it2);
   }
   it = _credit_history.erase(_credit_history.begin(), it);
   
   // Insert current credits
   _credit_history.insert(it, new CreditMsg(time, last_credits - num_credits));

   // Propagate changes to other entries in credit history
   for ( ; it != _credit_history.end(); it++)
   {
      (*it)->_num_credits -= num_credits;
   }
}

void
CreditHistory::prune(UInt64 time)
{
   decreaseCredits(time, 0);
}

void
CreditHistory::receive(BufferManagementMsg* buffer_mangement_msg)
{
   CreditMsg* credit_msg = dynamic_cast<CreditMsg*>(buffer_mangement_msg);
   CreditMsg* latest_credit_msg = _credit_history.back();

   if (latest_credit_msg->_normalized_time < credit_msg->_normalized_time)
   {
      // Add the credits to the end of the history
      for (SInt32 i = 0; i < credit_msg->_num_credits; i++)
      {
         _credit_history.push_back(new CreditMsg(credit_msg->_normalized_time + i, \
                  latest_credit_msg->_num_credits + i + 1));
      }
   }
   else  // (latest_credit_msg->_normalized_time >= credit_msg->_normalized_time)
   {
      assert(_credit_history.size() == 1);
      for (SInt32 i = 0; i < credit_msg->_num_credits; i++)
      {
         if (latest_credit_msg->_normalized_time == (credit_msg->_normalized_time + i))
            latest_credit_msg->_num_credits += (i + 1);
         else if (latest_credit_msg->_normalized_time < (credit_msg->_normalized_time + i))
            _credit_history.push_back(new CreditMsg(credit_msg->_normalized_time + i, \
                     latest_credit_msg->_num_credits + \
                        (credit_msg->_normalized_time + i - latest_credit_msg->_normalized_time) ));
      }
      if (latest_credit_msg->_normalized_time >= (credit_msg->_normalized_time + credit_msg->_num_credits))
         latest_credit_msg->_num_credits += credit_msg->_num_credits;
   }
}
