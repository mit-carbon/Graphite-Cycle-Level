#include "on_off_history.h"

OnOffHistory::OnOffHistory(SInt32 size_buffer):
   FiniteBufferUsageHistory(size_buffer)
{
   _on_off_history.push_back(new OnOffMsg(0, true));
}

OnOffHistory::~OnOffHistory()
{}

bool
OnOffHistory::allocate(Flit* flit)
{
   // Not possible with packet buffer flow control
   assert(flit->_length == 1);

   list<OnOffMsg*>::iterator it = _on_off_history.begin();
   for ( ; it != _on_off_history.end(); it ++)
   {
      OnOffMsg* curr_msg = (OnOffMsg*) (*it);
      if (curr_msg->_on_off_status)
      {
         list<OnOffMsg*>::iterator next_it = it;
         next_it ++;
         if ( (curr_msg->_normalized_time >= flit->_normalized_time) || (next_it == _on_off_history.end()) )
         {
            UInt64 buffer_allocation_delay = (curr_msg->_normalized_time > flit->_normalized_time) ? \
                                             (curr_msg->_normalized_time - flit->_normalized_time) : 0;
            flit->_normalized_time += buffer_allocation_delay;
            
            // Update the history
            updateUsage(flit->_normalized_time);
            return true;
         }
         else // ( (curr_msg->_normalized_time < flit->_normalized_time) && (next_it != _on_off_history.end()) )
         {
            OnOffMsg* next_msg = (OnOffMsg*) (*next_it);
            assert(!next_msg->_on_off_status);
            if (next_msg->_normalized_time > flit->_normalized_time)
            {
               // No buffer allocation delay
               
               // Update the history
               updateUsage(flit->_normalized_time);
               return true;
            }
         }
      }
   }
   
   return false;
}

void
OnOffHistory::updateUsage(UInt64 time)
{
   list<OnOffMsg*>::iterator it = _on_off_history.begin();
   list<OnOffMsg*>::iterator prev_it;
   for ( ; it != _on_off_history.end(); it++)
   {
      if ((*it)->_normalized_time > time)
         break;
      prev_it = it;
   }

   assert(it != _on_off_history.begin());

   // Get On off status
   bool on_off_status = (*prev_it)->_on_off_status;

   // Delete history till then
   for (list<OnOffMsg*>::iterator it2 = _on_off_history.begin(); it2 != it; it2 ++)
   {
      delete (*it2);
   }
   it = _on_off_history.erase(_on_off_history.begin(), it);

   // Insert current on-off status
   _on_off_history.insert(it, new OnOffMsg(time, on_off_status));
}

void
OnOffHistory::prune(UInt64 time)
{
   updateUsage(time);
}

void
OnOffHistory::receive(BufferManagementMsg* buffer_mangement_msg)
{
   OnOffMsg* msg = dynamic_cast<OnOffMsg*>(buffer_mangement_msg);
   OnOffMsg* latest_msg = _on_off_history.back();
   assert(msg->_on_off_status != latest_msg->_on_off_status);
         
   if (latest_msg->_normalized_time < msg->_normalized_time)
   {
      _on_off_history.push_back(new OnOffMsg(msg->_normalized_time, msg->_on_off_status));
   }
   else // latest_msg->_normalized_time >= msg->_normalized_time
   {
      assert(_on_off_history.size() == 1);
      latest_msg->_on_off_status = msg->_on_off_status;
   }
}
