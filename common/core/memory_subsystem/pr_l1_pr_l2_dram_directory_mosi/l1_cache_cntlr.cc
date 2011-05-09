#include "l1_cache_cntlr.h"
#include "l2_cache_cntlr.h" 
#include "memory_manager.h"
#include "event.h"

namespace PrL1PrL2DramDirectoryMOSI
{

L1CacheCntlr::L1CacheCntlr(core_id_t core_id,
      MemoryManager* memory_manager,
      UInt32 cache_block_size,
      UInt32 l1_icache_size, UInt32 l1_icache_associativity,
      std::string l1_icache_replacement_policy,
      UInt32 l1_dcache_size, UInt32 l1_dcache_associativity,
      std::string l1_dcache_replacement_policy,
      ShmemPerfModel* shmem_perf_model) :
   m_memory_manager(memory_manager),
   m_l2_cache_cntlr(NULL),
   m_core_id(core_id),
   m_cache_block_size(cache_block_size),
   m_shmem_perf_model(shmem_perf_model)
{
   m_l1_icache = new Cache("L1-I",
         l1_icache_size,
         l1_icache_associativity, 
         m_cache_block_size,
         l1_icache_replacement_policy,
         CacheBase::PR_L1_CACHE);
   m_l1_dcache = new Cache("L1-D",
         l1_dcache_size,
         l1_dcache_associativity, 
         m_cache_block_size,
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
}

void
L1CacheCntlr::deinitializeMissStatusMaps()
{
   assert(m_miss_status_maps[MemComponent::L1_ICACHE].empty());
   assert(m_miss_status_maps[MemComponent::L1_DCACHE].empty());
}

void
L1CacheCntlr::processMemOpFromCore(
      UInt32 memory_access_id,
      MemComponent::component_t mem_component,
      Core::lock_signal_t lock_signal,
      Core::mem_op_t mem_op_type,
      IntPtr ca_address, UInt32 offset,
      Byte* data_buf, UInt32 data_length,
      bool modeled)
{
   LOG_PRINT("processMemOpFromCore(), lock_signal(%u), mem_op_type(%u), ca_address(0x%x)",
         lock_signal, mem_op_type, ca_address);

   if (lock_signal != Core::UNLOCK)
      acquireLock(mem_component);

   if (operationPermissibleinL1Cache(mem_component,
                                     ca_address, mem_op_type,
                                     modeled, true /* update_cache_counters */))
   {
      // Increment Shared Mem Perf model cycle counts
      // L1 Cache
      getMemoryManager()->incrCycleCount(mem_component, CachePerfModel::ACCESS_CACHE_DATA_AND_TAGS);
      if (mem_op_type == Core::WRITE)
         getMemoryManager()->incrCycleCount(MemComponent::L2_CACHE, CachePerfModel::ACCESS_CACHE_DATA_AND_TAGS);

      accessCache(mem_component, mem_op_type, ca_address, offset, data_buf, data_length);
              
      if (lock_signal != Core::LOCK)
         releaseLock(mem_component);
   
      // Complete Cache Operation
      completeMemOpFromCore(memory_access_id);
      return;
   }

   getMemoryManager()->incrCycleCount(mem_component, CachePerfModel::ACCESS_CACHE_TAGS);
   
   LOG_ASSERT_ERROR(lock_signal != Core::UNLOCK, "Expected to find address(0x%x) in L1 Cache", ca_address);

   m_l2_cache_cntlr->acquireLock();

   ShmemMsg::msg_t shmem_msg_type = getShmemMsgType(mem_op_type);

   if (m_l2_cache_cntlr->processShmemReqFromL1Cache(mem_component, shmem_msg_type, ca_address, modeled))
   {
      m_l2_cache_cntlr->releaseLock();
      
      // Increment Shared Mem Perf model cycle counts
      // L2 Cache
      getMemoryManager()->incrCycleCount(MemComponent::L2_CACHE, CachePerfModel::ACCESS_CACHE_DATA_AND_TAGS);
      // L1 Cache
      getMemoryManager()->incrCycleCount(mem_component, CachePerfModel::ACCESS_CACHE_DATA_AND_TAGS);

      accessCache(mem_component, mem_op_type, ca_address, offset, data_buf, data_length);

      if (lock_signal != Core::LOCK)
         releaseLock(mem_component);
   
      // Complete Cache Operation
      completeMemOpFromCore(memory_access_id);
      return;
   }

   // Increment shared mem perf model cycle counts
   getMemoryManager()->incrCycleCount(MemComponent::L2_CACHE, CachePerfModel::ACCESS_CACHE_TAGS);
   
   m_l2_cache_cntlr->releaseLock();
   releaseLock(mem_component);
  
   L1MissStatus* l1_miss_status = new L1MissStatus(ca_address,
                                                   memory_access_id,
                                                   lock_signal, mem_op_type,
                                                   offset,
                                                   data_buf, data_length,
                                                   modeled);
   m_miss_status_maps[mem_component].insert(l1_miss_status);
   
   // Send out a request to the network thread for the cache data
   ShmemMsg shmem_msg(shmem_msg_type, mem_component, MemComponent::L2_CACHE,
         m_core_id, INVALID_CORE_ID, false, ca_address);
   getMemoryManager()->sendMsg(m_core_id, shmem_msg);

   if (Config::getSingleton()->getSimulationMode() != Config::CYCLE_ACCURATE)
   {
      waitForSimThread();
      reprocessMemOpFromCore(mem_component, l1_miss_status);
   }
}

void
L1CacheCntlr::reprocessMemOpFromCore(
      MemComponent::component_t mem_component,
      L1MissStatus* l1_miss_status)
{
   LOG_PRINT("reprocessMemOpFromCore() start");

   if (l1_miss_status->_lock_signal != Core::UNLOCK)
      acquireLock(mem_component);

   if (Config::getSingleton()->getSimulationMode() != Config::CYCLE_ACCURATE)
   {
      // Wake up the sim thread after acquiring the lock
      wakeUpSimThread();
   }

   assert(operationPermissibleinL1Cache(mem_component,
          l1_miss_status->_address, l1_miss_status->_mem_op_type,
          l1_miss_status->_modeled, false /* update_cache_counters */));

   accessCache(mem_component, 
               l1_miss_status->_mem_op_type,
               l1_miss_status->_address, l1_miss_status->_offset,
               l1_miss_status->_data_buf, l1_miss_status->_data_length);
 
   if (l1_miss_status->_lock_signal != Core::LOCK)
      releaseLock(mem_component);

   SInt32 memory_access_id = l1_miss_status->_memory_access_id;
   // Remove the MissStatus structure
   m_miss_status_maps[mem_component].erase(l1_miss_status);
   delete l1_miss_status;
 
   LOG_PRINT("reprocessMemOpFromCore() end");
 
   // Complete Cache Operation
   completeMemOpFromCore(memory_access_id);
}

void
L1CacheCntlr::completeMemOpFromCore(UInt32 memory_access_id)
{
   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) << getMemoryManager()->getCore() << memory_access_id; 
   EventCompleteCacheAccess* event = new EventCompleteCacheAccess(getShmemPerfModel()->getCycleCount(),
                                                                  event_args);
   Event::processInOrder(event, getMemoryManager()->getCore()->getId(), EventQueue::ORDERED);
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
         m_l2_cache_cntlr->acquireLock();
         m_l2_cache_cntlr->writeCacheBlock(ca_address, offset, data_buf, data_length);
         m_l2_cache_cntlr->releaseLock();
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
   // TODO: Verify why this works
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
L1CacheCntlr::acquireLock(MemComponent::component_t mem_component)
{
   switch(mem_component)
   {
      case MemComponent::L1_ICACHE:
         m_l1_icache_lock.acquire();
         break;
      case MemComponent::L1_DCACHE:
         m_l1_dcache_lock.acquire();
         break;
      default:
         LOG_PRINT_ERROR("Unrecognized mem_component(%u)", mem_component);
         break;
   }

}

void
L1CacheCntlr::releaseLock(MemComponent::component_t mem_component)
{
   switch(mem_component)
   {
      case MemComponent::L1_ICACHE:
         m_l1_icache_lock.release();
         break;
      case MemComponent::L1_DCACHE:
         m_l1_dcache_lock.release();
         break;
      default:
         LOG_PRINT_ERROR("Unrecognized mem_component(%u)", mem_component);
         break;
   }
}

void
L1CacheCntlr::signalDataReady(MemComponent::component_t mem_component, IntPtr address)
{
   if (Config::getSingleton()->getSimulationMode() != Config::CYCLE_ACCURATE)
   {
      getShmemPerfModel()->setCycleCount(ShmemPerfModel::_APP_THREAD,
            getShmemPerfModel()->getCycleCount());
      wakeUpAppThread();
      waitForAppThread();
   }
   else // (mode == CYCLE_ACCURATE)
   {
      L1MissStatus* l1_miss_status = (L1MissStatus*) m_miss_status_maps[mem_component].get(address);
      assert(l1_miss_status);
      reprocessMemOpFromCore(mem_component, l1_miss_status);
   }
}

}
