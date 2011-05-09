#pragma once

#include <map>
#include <list>

// Forward declarations
namespace PrL1PrL2DramDirectoryMSI
{
   class L1CacheCntlr;
   class MemoryManager;
}

#include "cache.h"
#include "pr_l2_cache_block_info.h"
#include "address_home_lookup.h"
#include "shmem_msg.h"
#include "mem_component.h"
#include "semaphore.h"
#include "lock.h"
#include "fixed_types.h"
#include "shmem_perf_model.h"
#include "miss_status.h"
#include "event.h"
#include "queue_model_simple.h"

namespace PrL1PrL2DramDirectoryMSI
{
   class L2CacheCntlr
   {
   private:
      enum ExternalEvents
      {
         L2_CACHE_ACCESS_MSG = 100
      };

      // Data Members
      MemoryManager* m_memory_manager;
      Cache* m_l2_cache;
      L1CacheCntlr* m_l1_cache_cntlr;
      AddressHomeLookup* m_dram_directory_home_lookup;

      // Map of L2 Miss Accesses
      MissStatusMap m_miss_status_map;

      // L2 Cache Contention Model
      QueueModelSimple* m_l2_cache_contention_model;

      // List of Pending Requests from Dram Directory
      std::list<std::pair<core_id_t,ShmemMsg*> > m_pending_dram_directory_req_list;

      // L2 Cache meta-data operations
      CacheState::cstate_t getCacheState(PrL2CacheBlockInfo* l2_cache_block_info);
      void setCacheState(PrL2CacheBlockInfo* l2_cache_block_info, CacheState::cstate_t cstate);

      // L2 Cache data operations
      void invalidateCacheBlock(IntPtr address);
      void retrieveCacheBlock(IntPtr address, Byte* data_buf);
      PrL2CacheBlockInfo* insertCacheBlock(IntPtr address, CacheState::cstate_t cstate, Byte* data_buf);

      // L1 Cache data manipulations
      void setCacheStateInL1(MemComponent::component_t mem_component, IntPtr address, CacheState::cstate_t cstate);
      void invalidateCacheBlockInL1(MemComponent::component_t mem_component, IntPtr address);
      void insertCacheBlockInL1(MemComponent::component_t mem_component, IntPtr address, PrL2CacheBlockInfo* l2_cache_block_info, CacheState::cstate_t cstate, Byte* data_buf);

      // Process Request from L1 Cache
      void processExReqFromL1Cache(ShmemMsg* shmem_msg);
      void processShReqFromL1Cache(ShmemMsg* shmem_msg);
      // Check if msg from L1 ends in the L2 cache
      bool shmemReqEndsInL2Cache(ShmemMsg::msg_t msg_type, CacheState::cstate_t cstate, bool modeled);

      // Schedule Requests
      void scheduleRequest(UInt64 time, core_id_t sender, ShmemMsg* shmem_msg);

      // Process Request from Dram Dir
      void processExRepFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg);
      void processShRepFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg);
      void processInvReqFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg);
      void processFlushReqFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg);
      void processWbReqFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg);

      PrL2CacheBlockInfo* getCacheBlockInfo(IntPtr address);

      // Cache Block Size
      core_id_t getCoreId();
      UInt32 getCacheBlockSize();
      MemoryManager* getMemoryManager() { return m_memory_manager; }
      bool isLocked();

      // Dram Directory Home Lookup
      core_id_t getHome(IntPtr address) { return m_dram_directory_home_lookup->getHome(address); }

   public:

      L2CacheCntlr(MemoryManager* memory_manager,
                   L1CacheCntlr* l1_cache_cntlr,
                   AddressHomeLookup* dram_directory_home_lookup,
                   UInt32 cache_block_size,
                   UInt32 l2_cache_size, UInt32 l2_cache_associativity,
                   std::string l2_cache_replacement_policy);
      ~L2CacheCntlr();

      Cache* getL2Cache() { return m_l2_cache; }

      // Handle Request from L1 Cache - This is done for better simulator performance
      bool processShmemReqFromL1Cache(MemComponent::component_t req_mem_component, ShmemMsg::msg_t msg_type, IntPtr address, bool modeled);
      // Write-through Cache. Hence needs to be written by user thread
      void writeCacheBlock(IntPtr address, UInt32 offset, Byte* data_buf, UInt32 data_length);

      // Handle message from L1 Cache
      void handleMsgFromL1Cache(ShmemMsg* shmem_msg);
      void __handleMsgFromL1Cache(ShmemMsg* shmem_msg);

      // Handle message from Dram Dir
      void handleMsgFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg);
      void __handleMsgFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg);
      
      // Schedule the Next Pending Request from Dram Directory
      void scheduleNextPendingRequest(UInt64 time);
      
      // For setting the Event Time correctly
      ShmemPerfModel* getShmemPerfModel();
   };

   void handleL2CacheAccessMsg(Event* event);

}
