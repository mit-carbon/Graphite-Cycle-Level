#pragma once

#include <map>

// Forward Decls
namespace PrL1PrL2DramDirectoryMSI
{
   class MemoryManager;
}

#include "dram_perf_model.h"
#include "shmem_perf_model.h"
#include "shmem_msg.h"
#include "fixed_types.h"

namespace PrL1PrL2DramDirectoryMSI
{
   class DramCntlr
   {
      public:
         typedef enum
         {
            READ = 0,
            WRITE,
            NUM_ACCESS_TYPES
         } access_t;

      private:
         MemoryManager* m_memory_manager;
         std::map<IntPtr, Byte*> m_data_map;
         DramPerfModel* m_dram_perf_model;

         typedef std::map<IntPtr,UInt64> AccessCountMap;
         AccessCountMap* m_dram_access_count;

         // Get/Put Data From/To Dram
         void getDataFromDram(core_id_t sender, ShmemMsg* shmem_msg);
         void putDataToDram(core_id_t sender, ShmemMsg* shmem_msg);
         
         UInt32 getCacheBlockSize();
         MemoryManager* getMemoryManager() { return m_memory_manager; }
         ShmemPerfModel* getShmemPerfModel();
         UInt64 runDramPerfModel(core_id_t requester);

         void addToDramAccessCount(IntPtr address, access_t access_type);
         void printDramAccessCount();

      public:
         DramCntlr(MemoryManager* memory_manager,
               float dram_access_cost,
               float dram_bandwidth,
               bool dram_queue_model_enabled);

         ~DramCntlr();

         DramPerfModel* getDramPerfModel() { return m_dram_perf_model; }

         void handleMsgFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg);
   };
}
