#pragma once

// Forward declaration
namespace PrL1PrL2DramDirectoryMSI
{
   class L2CacheCntlr;
   class MemoryManager;
}

#include "core.h"
#include "cache.h"
#include "pr_l1_cache_block_info.h"
#include "shmem_msg.h"
#include "mem_component.h"
#include "semaphore.h"
#include "lock.h"
#include "fixed_types.h"
#include "shmem_perf_model.h"
#include "miss_status.h"

namespace PrL1PrL2DramDirectoryMSI
{
   class L1CacheCntlr
   {
      private:
         MemoryManager* m_memory_manager;
         Cache* m_l1_icache;
         Cache* m_l1_dcache;
         L2CacheCntlr* m_l2_cache_cntlr;

         MissStatusMaps m_miss_status_maps;

         // States if the private caches are locked
         bool m_locked;

         // Private Functions
         void accessCache(MemComponent::component_t mem_component,
               Core::mem_op_t mem_op_type, 
               IntPtr ca_address, UInt32 offset,
               Byte* data_buf, UInt32 data_length);
         bool operationPermissibleinL1Cache(
               MemComponent::component_t mem_component, 
               IntPtr address, Core::mem_op_t mem_op_type,
               bool modeled, bool update_cache_counters);

         Cache* getL1Cache(MemComponent::component_t mem_component);
         ShmemMsg::msg_t getShmemMsgType(Core::mem_op_t mem_op_type);

         // Miss Status Maps
         void initializeMissStatusMaps();
         void deinitializeMissStatusMaps();

         // Get Cache Block Size
         core_id_t getCoreId();
         UInt32 getCacheBlockSize();
         MemoryManager* getMemoryManager() { return m_memory_manager; }
         ShmemPerfModel* getShmemPerfModel();

         // Initiate Actual Cache Access
         void doInitiateCacheAccess(MemComponent::component_t mem_component,
                                    UInt32 memory_access_id,
                                    Core::lock_signal_t lock_signal,
                                    Core::mem_op_t mem_op_type, 
                                    IntPtr ca_address, UInt32 offset,
                                    Byte* data_buf, UInt32 data_length,
                                    bool modeled,
                                    L1MissStatus* l1_miss_status);
         // Complete Cache Request
         void completeCacheRequest(MemComponent::component_t mem_component, UInt32 memory_access_id, L1MissStatus* l1_miss_status);
         // Process Next Cache Request 
         void processNextCacheRequest(MemComponent::component_t mem_component, IntPtr address);

      public:
         L1CacheCntlr(MemoryManager* memory_manager,
                      UInt32 cache_block_size,
                      UInt32 l1_icache_size, UInt32 l1_icache_associativity,
                      std::string l1_icache_replacement_policy,
                      UInt32 l1_dcache_size, UInt32 l1_dcache_associativity,
                      std::string l1_dcache_replacement_policy);
         ~L1CacheCntlr();

         Cache* getL1ICache() { return m_l1_icache; }
         Cache* getL1DCache() { return m_l1_dcache; }

         void setL2CacheCntlr(L2CacheCntlr* l2_cache_cntlr);

         // Called from memory manager to start L1 cache access
         void initiateCacheAccess(MemComponent::component_t mem_component,
                                  UInt32 memory_access_id,
                                  Core::lock_signal_t lock_signal,
                                  Core::mem_op_t mem_op_type, 
                                  IntPtr ca_address, UInt32 offset,
                                  Byte* data_buf, UInt32 data_length,
                                  bool modeled);
         // Called from memory manager to re-start L1 cache access
         void reInitiateCacheAccess(MemComponent::component_t mem_component, L1MissStatus* l1_miss_status);
         
         // Called from L2 cache cntlr to indicate that a memory request has been processed
         void signalDataReady(MemComponent::component_t mem_component, IntPtr address);

         void insertCacheBlock(MemComponent::component_t mem_component,
               IntPtr address, CacheState::cstate_t cstate, Byte* data_buf,
               bool* eviction_ptr, IntPtr* evict_address_ptr);

         CacheState::cstate_t getCacheState(
               MemComponent::component_t mem_component, IntPtr address);
         void setCacheState(MemComponent::component_t mem_component,
               IntPtr address, CacheState::cstate_t cstate);
         void invalidateCacheBlock(MemComponent::component_t mem_component, IntPtr address);

         void acquireLock();
         void releaseLock();
         bool isLocked();
   };
}
