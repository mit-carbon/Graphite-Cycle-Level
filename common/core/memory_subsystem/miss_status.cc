#include "miss_status.h"
#include "log.h"

bool MissStatusMap::insert(MissStatus* miss_status)
{  
   IntPtr address = miss_status->_address;
   MissStatus* curr_miss_status = _miss_status_map[address];
   if (curr_miss_status)
   {
      LOG_PRINT_ERROR("Currently not allowed");
      curr_miss_status->_next = miss_status;
      return false;
   }
   else
   {
      _miss_status_map[address] = miss_status;
      return true;
   }
}

void MissStatusMap::erase(MissStatus* miss_status)
{
   IntPtr address = miss_status->_address;
   if (miss_status->_next)
   {
      LOG_PRINT_ERROR("Currently not allowed");
      _miss_status_map[address] = miss_status->_next;
   }
   else
   {
      _miss_status_map.erase(address);
   }
}
