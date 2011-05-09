#pragma once

#include <string>
#include <map>
#include <vector>
#include <set>

// Forward Decls
namespace PrL1PrL2DramDirectoryMSI
{
   class MemoryManager;
}

#include "directory.h"
#include "shmem_perf_model.h"

namespace PrL1PrL2DramDirectoryMSI
{
   class DramDirectoryCache
   {
   private:
      MemoryManager* m_memory_manager;
      Directory* m_directory;
      std::vector<DirectoryEntry*> m_replaced_directory_entry_list;
      
      std::map<IntPtr,UInt64> m_address_map;
      std::vector<std::map<IntPtr,UInt64> > m_set_specific_address_map;
      std::map<IntPtr,UInt64> m_replaced_address_map;
      std::vector<UInt64> m_set_replacement_histogram;

      UInt32 m_total_entries;
      UInt32 m_associativity;

      UInt32 m_num_sets;

      UInt32 m_log_num_sets;
      UInt32 m_log_cache_block_size;
      UInt32 m_log_num_cores;
      UInt32 m_log_num_dram_cntlrs;

      UInt32 m_log_stack_size;

      UInt64 m_dram_directory_cache_access_delay_in_ns;
      UInt64 m_dram_directory_cache_access_delay_in_clock_cycles;

      MemoryManager* getMemoryManager() { return m_memory_manager; }
      UInt32 getCacheBlockSize();
      ShmemPerfModel* getShmemPerfModel();

      void initializeParameters(UInt32 num_dram_cntlrs);
      void splitAddress(IntPtr address, IntPtr& tag, UInt32& set_index);
      
      IntPtr computeSetIndex(IntPtr address);
   
   public:

      DramDirectoryCache(
            MemoryManager* memory_manager,
            std::string directory_type_str,
            UInt32 total_entries,
            UInt32 associativity,
            UInt32 cache_block_size,
            UInt32 max_hw_sharers,
            UInt32 max_num_sharers,
            UInt32 num_dram_cntlrs,
            UInt64 dram_directory_cache_access_delay_in_ns);
      ~DramDirectoryCache();

      DirectoryEntry* getDirectoryEntry(IntPtr address);
      DirectoryEntry* replaceDirectoryEntry(IntPtr replaced_address, IntPtr address);
      void invalidateDirectoryEntry(IntPtr address);
      void getReplacementCandidates(IntPtr address, std::vector<DirectoryEntry*>& replacement_candidate_list);

      UInt64 getAccessDelay() { return m_dram_directory_cache_access_delay_in_clock_cycles; }

      void updateInternalVariablesOnFrequencyChange(volatile float core_frequency);

      void outputSummary(std::ostream& os);
      static void dummyOutputSummary(std::ostream& os);
   };
}
