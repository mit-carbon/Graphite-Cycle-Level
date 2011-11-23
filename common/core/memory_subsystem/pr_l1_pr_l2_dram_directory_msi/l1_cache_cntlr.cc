#include "l1_cache_cntlr.h"
#include "l2_cache_cntlr.h" 
#include "memory_manager.h"
#include "event.h"

namespace PrL1PrL2DramDirectoryMSI
{

L1CacheCntlr::L1CacheCntlr(MemoryManager* memory_manager,
                           UInt32 cache_block_size,
                           UInt32 l1_icache_size, UInt32 l1_icache_associativity,
                           std::string l1_icache_replacement_policy,
                           UInt32 l1_dcache_size, UInt32 l1_dcache_associativity,
                           std::string l1_dcache_replacement_policy):
   m_memory_manager(memory_manager),
   m_l2_cache_cntlr(NULL),
   m_locked(false)
{
   m_l1_icache = new Cache("L1-I",
         l1_icache_size,
         l1_icache_associativity, 
         cache_block_size,
         l1_icache_replacement_policy,
         CacheBase::PR_L1_CACHE);
   m_l1_dcache = new Cache("L1-D",
         l1_dcache_size,
         l1_dcache_associativity, 
         cache_block_size,
         l1_dcache_replacement_policy,
         CacheBase::PR_L1_CACHE);

   initializeMissStatusMaps();
}

L1CacheCntlr::~L1CacheCntlr()
{
   deinitializeMissStatusMaps();
   delete m_l1_icache;
   delete m_l1_dcache;
} 

void
L1CacheCntlr::setL2CacheCntlr(L2CacheCntlr* l2_cache_cntlr)
{
   m_l2_cache_cntlr = l2_cache_cntlr;
}

void
L1CacheCntlr::initializeMissStatusMaps()
{
   m_miss_status_maps.insert(make_pair<MemComponent::component_t, MissStatusMap>
                            (MemComponent::L1_ICACHE, MissStatusMap()));
   m_miss_status_maps.insert(make_pair<MemComponent::component_t, MissStatusMap>
                            (MemComponent::L1_DCACHE, MissStatusMap()));
   m_miss_status_maps[MemComponent::L1_ICACHE].print();
   m_miss_status_maps[MemComponent::L1_DCACHE].print();
}

void
L1CacheCntlr::deinitializeMissStatusMaps()
{
   LOG_PRINT("DeinitializeMissStatusMaps() start");
   m_miss_status_maps[MemComponent::L1_ICACHE].print();
   m_miss_status_maps[MemComponent::L1_DCACHE].print();
   assert(m_miss_status_maps[MemComponent::L1_ICACHE].empty());
   assert(m_miss_status_maps[MemComponent::L1_DCACHE].empty());
   LOG_PRINT("DeinitializeMissStatusMaps() end");
}

void
L1CacheCntlr::initiateCacheAccess(MemComponent::component_t mem_component,
                                  UInt32 memory_access_id,
                                  Core::lock_signal_t lock_signal,
                                  Core::mem_op_t mem_op_type,
                                  IntPtr ca_address, UInt32 offset,
                                  Byte* data_buf, UInt32 data_length,
                                  bool modeled)
{
   LOG_PRINT("initiateCacheAccess() [Core Id(%i), Memory Access Id(%u), Mem Component(%u), Lock Signal(%u), "
             "Mem Op Type(%u), CA-Address(%#lx), Offset(%u), Data Buf(%p), Data Length(%u), Modeled(%s)]",
             getCoreId(), memory_access_id, mem_component, lock_signal, mem_op_type,
             ca_address, offset, data_buf, data_length, modeled ? "TRUE" : "FALSE");
  
   if (m_miss_status_maps[mem_component].get(ca_address))
   { 
      // Insert it into the queue and wait till the miss is complete
      L1MissStatus* l1_miss_status = new L1MissStatus(ca_address,
                                                      memory_access_id,
                                                      lock_signal, mem_op_type,
                                                      offset,
                                                      data_buf, data_length,
                                                      modeled);
      m_miss_status_maps[mem_component].insert(l1_miss_status);
   }
   else
   {
      doInitiateCacheAccess(mem_component, memory_access_id,
                            lock_signal, mem_op_type, ca_address, offset, data_buf, data_length, modeled,
                            (L1MissStatus*) NULL);
   }
}

void
L1CacheCntlr::reInitiateCacheAccess(MemComponent::component_t mem_component, L1MissStatus* l1_miss_status)
{
   doInitiateCacheAccess(mem_component, l1_miss_status->_memory_access_id,
                         l1_miss_status->_lock_signal, l1_miss_status->_mem_op_type,
                         l1_miss_status->_address, l1_miss_status->_offset,
                         l1_miss_status->_data_buf, l1_miss_status->_data_length,
                         l1_miss_status->_modeled, l1_miss_status);
}

void
L1CacheCntlr::doInitiateCacheAccess(MemComponent::component_t mem_component,
                                    UInt32 memory_access_id,
                                    Core::lock_signal_t lock_signal,
                                    Core::mem_op_t mem_op_type,
                                    IntPtr address, UInt32 offset,
                                    Byte* data_buf, UInt32 data_length,
                                    bool modeled,
                                    L1MissStatus* l1_miss_status)
{
   LOG_PRINT("Core Id(%i): doInitiateCacheAccess() [Memory Access Id(%u), Mem Component(%u), Lock Signal(%u), "
             "Mem Op Type(%u), CA-Address(%#lx), Offset(%u), Data Buf(%p), Data Length(%u), Modeled(%s)], L1 Miss Status(%p)",
             getCoreId(), memory_access_id, mem_component, lock_signal, mem_op_type,
             address, offset, data_buf, data_length, modeled ? "TRUE" : "FALSE", l1_miss_status);
   
   assert((!l1_miss_status) || (l1_miss_status->_access_num == 1) || (l1_miss_status->_access_num == 2));
   bool update_cache_counters = ( (!l1_miss_status) || (l1_miss_status->_access_num == 1) );
   
   if (operationPermissibleinL1Cache(mem_component, address, mem_op_type, modeled, update_cache_counters))
   {
      // Increment Shared Mem Perf model cycle counts
      // L1 Cache
      getMemoryManager()->incrCycleCount(mem_component, CachePerfModel::ACCESS_CACHE_DATA_AND_TAGS);
      if (mem_op_type == Core::WRITE)
         getMemoryManager()->incrCycleCount(MemComponent::L2_CACHE, CachePerfModel::ACCESS_CACHE_DATA_AND_TAGS);

      accessCache(mem_component, mem_op_type, address, offset, data_buf, data_length);
 
      // Complete Cache Request
      completeCacheRequest(mem_component, memory_access_id, l1_miss_status);

      if (lock_signal == Core::LOCK)
         acquireLock();
      else if (lock_signal == Core::UNLOCK)
         releaseLock();

      return;
   }

   assert(update_cache_counters);
   LOG_ASSERT_ERROR(lock_signal != Core::UNLOCK, "Expected to find address(%#lx) in L1 Cache", address);

   // Invalidate the cache block before passing the request to L2 Cache
   invalidateCacheBlock(mem_component, address);

   ShmemMsg::msg_t shmem_msg_type = getShmemMsgType(mem_op_type);

   if (m_l2_cache_cntlr->processShmemReqFromL1Cache(mem_component, shmem_msg_type, address, modeled))
   {
      // Increment Shared Mem Perf model cycle counts
      // L1 Cache Tags
      getMemoryManager()->incrCycleCount(mem_component, CachePerfModel::ACCESS_CACHE_TAGS);
      // L2 Cache Data & Tags
      getMemoryManager()->incrCycleCount(MemComponent::L2_CACHE, CachePerfModel::ACCESS_CACHE_DATA_AND_TAGS);
      // L1 Cache Data & Tags
      getMemoryManager()->incrCycleCount(mem_component, CachePerfModel::ACCESS_CACHE_DATA_AND_TAGS);

      accessCache(mem_component, mem_op_type, address, offset, data_buf, data_length);

      // Complete Cache Request
      completeCacheRequest(mem_component, memory_access_id, l1_miss_status);
      
      if (lock_signal == Core::LOCK)
         acquireLock();
      else if (lock_signal == Core::UNLOCK)
         releaseLock();
      
      return;
   }

   if (!l1_miss_status)
   {
      // Insert it into the queue and wait till the miss is complete
      l1_miss_status = new L1MissStatus(address,
                                        memory_access_id,
                                        lock_signal, mem_op_type,
                                        offset,
                                        data_buf, data_length,
                                        modeled);
      m_miss_status_maps[mem_component].insert(l1_miss_status);
   }

   // Increment the Access Num 
   l1_miss_status->_access_num ++; 

   // Send the request to the L2 Cache
   ShmemMsg shmem_msg(shmem_msg_type,
         mem_component, MemComponent::L2_CACHE,
         getCoreId(),
         address,
         false /* reply_expected */,
         NULL, 0);
   m_l2_cache_cntlr->handleMsgFromL1Cache(&shmem_msg);
}

void
L1CacheCntlr::completeCacheRequest(MemComponent::component_t mem_component,
                                   UInt32 memory_access_id,
                                   L1MissStatus* l1_miss_status)
{
   LOG_PRINT("completeCacheRequest(): [Mem Component(%u), Memory Access Id(%u), L1 Miss Status(%p)]",
             mem_component, memory_access_id, l1_miss_status);

   // Send Reply to Core
   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) << getMemoryManager()->getCore() << memory_access_id;
   EventCompleteCacheAccess* event = new EventCompleteCacheAccess(getShmemPerfModel()->getCycleCount(),
                                                                  event_args);
   Event::processInOrder(event, getCoreId(), EventQueue::ORDERED);
 
   if (l1_miss_status)
   {
      IntPtr address = l1_miss_status->_address; 
      
      // Erase the cache request from the queue
      m_miss_status_maps[mem_component].erase(l1_miss_status);
      delete l1_miss_status;

      // Process next cache request to same address
      processNextCacheRequest(mem_component, address);
   }
}

void
L1CacheCntlr::processNextCacheRequest(MemComponent::component_t mem_component, IntPtr address)
{
   L1MissStatus* l1_miss_status = (L1MissStatus*) m_miss_status_maps[mem_component].get(address);

   if (l1_miss_status)
   {
      UInt64 time = getShmemPerfModel()->getCycleCount() + 1;

      UnstructuredBuffer* event_args = new UnstructuredBuffer();
      (*event_args) << getMemoryManager() << mem_component << l1_miss_status;
      
      EventReInitiateCacheAccess* event = new EventReInitiateCacheAccess(time, event_args);
      Event::processInOrder(event, getCoreId(), EventQueue::ORDERED);
   }
}

void
L1CacheCntlr::accessCache(MemComponent::component_t mem_component,
      Core::mem_op_t mem_op_type, IntPtr ca_address, UInt32 offset,
      Byte* data_buf, UInt32 data_length)
{
   Cache* l1_cache = getL1Cache(mem_component);
   switch (mem_op_type)
   {
      case Core::READ:
      case Core::READ_EX:
         l1_cache->accessSingleLine(ca_address + offset, Cache::LOAD, data_buf, data_length);
         break;

      case Core::WRITE:
         l1_cache->accessSingleLine(ca_address + offset, Cache::STORE, data_buf, data_length);
         // Write-through cache - Write the L2 Cache also
         m_l2_cache_cntlr->writeCacheBlock(ca_address, offset, data_buf, data_length);
         break;

      default:
         LOG_PRINT_ERROR("Unsupported Mem Op Type: %u", mem_op_type);
         break;
   }
}

bool
L1CacheCntlr::operationPermissibleinL1Cache(
      MemComponent::component_t mem_component, 
      IntPtr address, Core::mem_op_t mem_op_type,
      bool modeled, bool update_cache_counters)
{
   bool cache_hit = false;
   CacheState::cstate_t cstate = getCacheState(mem_component, address);
   
   switch (mem_op_type)
   {
      case Core::READ:
         cache_hit = CacheState(cstate).readable();
         break;

      case Core::READ_EX:
      case Core::WRITE:
         cache_hit = CacheState(cstate).writable();
         break;

      default:
         LOG_PRINT_ERROR("Unsupported mem_op_type: %u", mem_op_type);
         break;
   }

   if (modeled && update_cache_counters)
   {
      // Update the Cache Counters
      getL1Cache(mem_component)->updateCounters(cache_hit);
   }

   return cache_hit;
}

void
L1CacheCntlr::insertCacheBlock(MemComponent::component_t mem_component,
      IntPtr address, CacheState::cstate_t cstate, Byte* data_buf,
      bool* eviction_ptr, IntPtr* evict_address_ptr)
{
   __attribute(__unused__) PrL1CacheBlockInfo evict_block_info;
   __attribute(__unused__) Byte evict_buf[getCacheBlockSize()];

   Cache* l1_cache = getL1Cache(mem_component);
   l1_cache->insertSingleLine(address, data_buf,
         eviction_ptr, evict_address_ptr, &evict_block_info, evict_buf);
   setCacheState(mem_component, address, cstate);
}

CacheState::cstate_t
L1CacheCntlr::getCacheState(MemComponent::component_t mem_component, IntPtr address)
{
   Cache* l1_cache = getL1Cache(mem_component);

   PrL1CacheBlockInfo* l1_cache_block_info = (PrL1CacheBlockInfo*) l1_cache->peekSingleLine(address);
   return (l1_cache_block_info == NULL) ? CacheState::INVALID : l1_cache_block_info->getCState(); 
}

void
L1CacheCntlr::setCacheState(MemComponent::component_t mem_component, IntPtr address, CacheState::cstate_t cstate)
{
   Cache* l1_cache = getL1Cache(mem_component);

   PrL1CacheBlockInfo* l1_cache_block_info = (PrL1CacheBlockInfo*) l1_cache->peekSingleLine(address);
   assert(l1_cache_block_info);

   l1_cache_block_info->setCState(cstate);
}

void
L1CacheCntlr::invalidateCacheBlock(MemComponent::component_t mem_component, IntPtr address)
{
   Cache* l1_cache = getL1Cache(mem_component);

   l1_cache->invalidateSingleLine(address);
}

ShmemMsg::msg_t
L1CacheCntlr::getShmemMsgType(Core::mem_op_t mem_op_type)
{
   switch(mem_op_type)
   {
   case Core::READ:
      return ShmemMsg::SH_REQ;

   case Core::READ_EX:
   case Core::WRITE:
      return ShmemMsg::EX_REQ;

   default:
      LOG_PRINT_ERROR("Unsupported Mem Op Type(%u)", mem_op_type);
      return ShmemMsg::INVALID_MSG_TYPE;
   }
}

Cache*
L1CacheCntlr::getL1Cache(MemComponent::component_t mem_component)
{
   switch(mem_component)
   {
   case MemComponent::L1_ICACHE:
      return m_l1_icache;

   case MemComponent::L1_DCACHE:
      return m_l1_dcache;

   default:
      LOG_PRINT_ERROR("Unrecognized Memory Component(%u)", mem_component);
      return NULL;
   }
}

void
L1CacheCntlr::acquireLock()
{
   LOG_PRINT("acquireLock()");
   m_locked = true;
}

void
L1CacheCntlr::releaseLock()
{
   LOG_PRINT("releaseLock()");
   m_locked = false;
   m_l2_cache_cntlr->scheduleNextPendingRequest(getShmemPerfModel()->getCycleCount());
}

bool
L1CacheCntlr::isLocked()
{
   return m_locked;
}

void
L1CacheCntlr::signalDataReady(MemComponent::component_t mem_component, IntPtr address)
{
   L1MissStatus* l1_miss_status = (L1MissStatus*) m_miss_status_maps[mem_component].get(address);
   assert(l1_miss_status);
   reInitiateCacheAccess(mem_component, l1_miss_status);
}

core_id_t
L1CacheCntlr::getCoreId()
{
   return getMemoryManager()->getCore()->getId();
}

UInt32
L1CacheCntlr::getCacheBlockSize()
{
   return getMemoryManager()->getCacheBlockSize();
}

ShmemPerfModel*
L1CacheCntlr::getShmemPerfModel()
{
   return getMemoryManager()->getShmemPerfModel();
}

}
