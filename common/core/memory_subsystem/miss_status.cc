#include "miss_status.h"
#include "log.h"

MissStatus*
MissStatusMap::get(IntPtr address)
{
   LOG_PRINT("MissStatusMap(%p): Address(%#lx), get()", this, address);
   MissStatusInfo::iterator it = _miss_status_info.find(address);
   if (it == _miss_status_info.end())
      return (MissStatus*) NULL;
   
   MissStatusQueue* miss_status_queue = (*it).second;
   assert(miss_status_queue && !miss_status_queue->empty());
   return miss_status_queue->front();
}

size_t
MissStatusMap::size(IntPtr address)
{
   LOG_PRINT("MissStatusMap(%p): Address(%#lx), size()", this, address);
   MissStatusInfo::iterator it = _miss_status_info.find(address);
   
   if (it == _miss_status_info.end())
      return (size_t) 0;
   
   MissStatusQueue* miss_status_queue = (*it).second;   
   assert(miss_status_queue && !miss_status_queue->empty());
   return miss_status_queue->size();
}

bool
MissStatusMap::insert(MissStatus* miss_status)
{
   LOG_PRINT("MissStatusMap(%p): Address(%#lx), insert(), size(%u)", this, miss_status->_address, _miss_status_info.size());

   IntPtr address = miss_status->_address;
   
   MissStatusInfo::iterator it = _miss_status_info.find(address);
   if (it == _miss_status_info.end()) // First outstanding miss on this address
   {
      MissStatusQueue* miss_status_queue = new MissStatusQueue();
      miss_status_queue->push(miss_status);
      _miss_status_info.insert(make_pair(address, miss_status_queue));
      LOG_PRINT("Size(%u)", _miss_status_info.size());
      return false;
   }
   else
   {
      MissStatusQueue* miss_status_queue = (*it).second;
      assert(miss_status_queue && !miss_status_queue->empty());
      miss_status_queue->push(miss_status);
      return true;
   }
}

MissStatus*
MissStatusMap::erase(MissStatus* miss_status)
{
   LOG_PRINT("MissStatusMap(%p): Address(%#lx), erase(), size(%u)",
         this, miss_status->_address, _miss_status_info.size());
   
   IntPtr address = miss_status->_address;

   MissStatusInfo::iterator it = _miss_status_info.find(address);
   assert(it != _miss_status_info.end());
   
   MissStatusQueue* miss_status_queue = (*it).second;
   assert(miss_status_queue && !miss_status_queue->empty());
   assert(miss_status_queue->front() == miss_status);

   LOG_PRINT("mapsize(%u)", miss_status_queue->size());
   miss_status_queue->pop();
   LOG_PRINT("mapsize(%u)", miss_status_queue->size());
   
   if (miss_status_queue->empty())
   {
      _miss_status_info.erase(address);
      delete miss_status_queue;
      LOG_PRINT("Size(%u)", _miss_status_info.size());
      return (MissStatus*) NULL;
   }
   else // There are other outstanding misses
   {
      return miss_status_queue->front();
   }
}

bool
MissStatusMap::empty()
{
   return _miss_status_info.empty();
}

void
MissStatusMap::print()
{
   LOG_PRINT("Miss Status Map(%p): size(%u)", this, _miss_status_info.size());
   MissStatusInfo::iterator it = _miss_status_info.begin();
   for ( ; it != _miss_status_info.end(); it ++)
   {
      LOG_PRINT("Address(%#lx), Size(%u)", (*it).first, ((*it).second)->size());
   }
}
