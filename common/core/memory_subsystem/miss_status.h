#pragma once

#include <map>
#include <queue>
using std::map;
using std::queue;

#include "fixed_types.h"
#include "mem_component.h"
#include "core.h"
#include "log.h"

class MissStatus
{
public:
   MissStatus(IntPtr address)
      : _address(address)
   {}
   ~MissStatus()
   {}

   IntPtr _address;
};

class MissStatusMap
{
public:
   MissStatusMap() {}
   ~MissStatusMap() {}
   bool insert(MissStatus* miss_status);
   MissStatus* erase(MissStatus* miss_status);
   MissStatus* get(IntPtr address);
   size_t size(IntPtr address);
   bool empty();
   void print();

private:
   typedef queue<MissStatus*> MissStatusQueue;
   typedef map<IntPtr,MissStatusQueue*> MissStatusInfo;
   MissStatusInfo _miss_status_info;
};

typedef std::map<MemComponent::component_t, MissStatusMap> MissStatusMaps;

class L1MissStatus : public MissStatus
{
public:
   L1MissStatus(IntPtr address,
                SInt32 memory_access_id,
                Core::lock_signal_t lock_signal,
                Core::mem_op_t mem_op_type, 
                UInt32 offset,
                Byte* data_buf, UInt32 data_length,
                bool modeled)
      : MissStatus(address)
      , _memory_access_id(memory_access_id)
      , _lock_signal(lock_signal)
      , _mem_op_type(mem_op_type)
      , _offset(offset)
      , _data_buf(data_buf)
      , _data_length(data_length)
      , _modeled(modeled)
      , _access_num(1)
   {}
   ~L1MissStatus() {}

   UInt32 _memory_access_id;
   Core::lock_signal_t _lock_signal;
   Core::mem_op_t _mem_op_type;
   UInt32 _offset;
   Byte* _data_buf;
   UInt32 _data_length;
   bool _modeled;
   UInt32 _access_num;
};

class L2MissStatus : public MissStatus
{
public:
   L2MissStatus(IntPtr address, MemComponent::component_t mem_component)
      : MissStatus(address)
      , _mem_component(mem_component)
   {}
   ~L2MissStatus() {}

   MemComponent::component_t _mem_component;
};
