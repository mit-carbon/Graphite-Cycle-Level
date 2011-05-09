#pragma once
using namespace std;

#include "core.h"
#include "network.h"
#include "mem_component.h"
#include "shmem_perf_model.h"
#include "miss_status.h"

void MemoryManagerNetworkCallback(void* obj, NetPacket packet);

class MemoryManager
{
   public:
      enum CachingProtocol_t
      {
         PR_L1_PR_L2_DRAM_DIRECTORY_MSI = 0,
         PR_L1_PR_L2_DRAM_DIRECTORY_MOSI,
         NUM_CACHING_PROTOCOL_TYPES
      };

   private:
      Core* m_core;
      Network* m_network;
      ShmemPerfModel* m_shmem_perf_model;
      
      void parseMemoryControllerList(string& memory_controller_positions, vector<core_id_t>& core_list_from_cfg_file, SInt32 application_core_count);

   protected:
      Network* getNetwork() { return m_network; }

      vector<core_id_t> getCoreListWithMemoryControllers(void);
      void printCoreListWithMemoryControllers(vector<core_id_t>& core_list_with_memory_controllers);
   
   public:
      MemoryManager(Core* core, Network* network, ShmemPerfModel* shmem_perf_model):
         m_core(core), 
         m_network(network), 
         m_shmem_perf_model(shmem_perf_model)
      {}
      virtual ~MemoryManager() {}

      virtual void initiateCacheAccess(UInt64 time,
                                       MemComponent::component_t mem_component,
                                       UInt32 memory_access_id,
                                       Core::lock_signal_t lock_signal,
                                       Core::mem_op_t mem_op_type,
                                       IntPtr address, UInt32 offset,
                                       Byte* data_buf, UInt32 data_length,
                                       bool modeled) = 0;
      virtual void reInitiateCacheAccess(UInt64 time,
                                         MemComponent::component_t mem_component,
                                         MissStatus* miss_status) = 0;

      virtual void handleMsgFromNetwork(NetPacket& packet) = 0;

      // FIXME: Take this out of here
      virtual UInt32 getCacheBlockSize() = 0;

      virtual core_id_t getShmemRequester(const void* pkt_data) = 0;

      virtual void updateInternalVariablesOnFrequencyChange(volatile float frequency) = 0;

      virtual void enableModels() = 0;
      virtual void disableModels() = 0;
      virtual void resetModels() = 0;

      // Modeling
      virtual UInt32 getModeledLength(const void* pkt_data) = 0;
      virtual bool isModeled(const void* pkt_data) = 0;

      Core* getCore() { return m_core; }
      
      static CachingProtocol_t parseProtocolType(std::string& protocol_type);
      static MemoryManager* createMMU(std::string protocol_type,
            Core* core,
            Network* network, 
            ShmemPerfModel* shmem_perf_model);
      
      virtual void outputSummary(std::ostream& os) = 0;
      
      ShmemPerfModel* getShmemPerfModel() { return m_shmem_perf_model; }
};
