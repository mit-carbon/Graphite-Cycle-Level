#pragma once

#include <string>
#include <set>
using std::set;

// Forward Decls
namespace PrL1PrL2DramDirectoryMSI
{
   class MemoryManager;
}

#include "dram_directory_cache.h"
#include "req_queue_list.h"
#include "dram_cntlr.h"
#include "address_home_lookup.h"
#include "shmem_req.h"
#include "shmem_msg.h"
#include "mem_component.h"
#include "event.h"
#include "queue_model_simple.h"

namespace PrL1PrL2DramDirectoryMSI
{
   class DramDirectoryCntlr
   {
      private:
         enum ExternalEvents
         {
            DRAM_DIRECTORY_ACCESS_REQ = 101,
            DRAM_DIRECTORY_SCHEDULE_NEXT_REQ_FROM_L2_CACHE = 102,
            DRAM_DIRECTORY_HANDLE_NEXT_REQ_FROM_L2_CACHE = 103
         };

         // Functional Models
         MemoryManager* m_memory_manager;
         DramDirectoryCache* m_dram_directory_cache;
         ReqQueueList* m_dram_directory_req_queue_list;

         // Outstanding DRAM requests
         set<IntPtr> m_dram_req_outstanding_set;

         core_id_t getCoreId();
         UInt32 getCacheBlockSize();
         MemoryManager* getMemoryManager() { return m_memory_manager; }

         // Dram Directory Contention Model
         QueueModelSimple* m_dram_directory_contention_model;

         // Private Functions
         DirectoryEntry* processDirectoryEntryAllocationReq(ShmemReq* shmem_req);
         void processNullifyReq(ShmemReq* shmem_req);

         void processExReqFromL2Cache(ShmemReq* shmem_req, Byte* cached_data_buf = NULL);
         void processShReqFromL2Cache(ShmemReq* shmem_req, Byte* cached_data_buf = NULL);
         void retrieveDataAndSendToL2Cache(ShmemMsg::msg_t reply_msg_type, core_id_t receiver, IntPtr address, Byte* cached_data_buf);

         void processInvRepFromL2Cache(core_id_t sender, ShmemMsg* shmem_msg);
         void processFlushRepFromL2Cache(core_id_t sender, ShmemMsg* shmem_msg);
         void processWbRepFromL2Cache(core_id_t sender, ShmemMsg* shmem_msg);

         // Get/Put Data From/To Dram
         void getDataFromDram(IntPtr address, core_id_t requester);
         void putDataToDram(IntPtr address, core_id_t requester, Byte* data_buf);

         // Schedule Next Dram Directory Access for the same address from the L2 Cache
         void scheduleNextReqFromL2Cache(IntPtr address);
         // Schedule next Dram Directory Access from Dram/L2 Cache
         void scheduleRequest(core_id_t sender, ShmemMsg* shmem_msg);
      
      public:
         DramDirectoryCntlr(MemoryManager* memory_manager,
                            UInt32 dram_directory_total_entries,
                            UInt32 dram_directory_associativity,
                            UInt32 cache_block_size,
                            UInt32 dram_directory_max_num_sharers,
                            UInt32 dram_directory_max_hw_sharers,
                            std::string dram_directory_type_str,
                            UInt64 dram_directory_cache_access_delay_in_ns,
                            UInt32 num_dram_cntlrs);
         ~DramDirectoryCntlr();

         // Msg from L2Cache
         void handleMsgFromL2Cache(core_id_t sender, ShmemMsg* shmem_msg);
         void __handleMsgFromL2Cache(core_id_t sender, ShmemMsg* shmem_msg);
         // Msg From Dram
         void handleMsgFromDram(core_id_t sender, ShmemMsg* shmem_msg);
         void __handleMsgFromDram(core_id_t sender, ShmemMsg* shmem_msg);
         // Process Next Req From L2 Cache
         void __scheduleNextReqFromL2Cache(IntPtr address);
         void handleNextReqFromL2Cache(IntPtr address);
         
         DramDirectoryCache* getDramDirectoryCache() { return m_dram_directory_cache; }
         
         ShmemPerfModel* getShmemPerfModel();
   };

   void handleDramDirectoryAccessReq(Event* event);
   void scheduleNextDramDirectoryAccessReqFromL2Cache(Event* event);
   void handleNextDramDirectoryAccessReqFromL2Cache(Event* event);

}
