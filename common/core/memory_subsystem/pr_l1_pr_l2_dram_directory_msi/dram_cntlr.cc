#include "dram_cntlr.h"
#include "memory_manager.h"
#include "core.h"
#include "clock_converter.h"
#include "log.h"

namespace PrL1PrL2DramDirectoryMSI
{

DramCntlr::DramCntlr(MemoryManager* memory_manager,
      float dram_access_cost,
      float dram_bandwidth,
      bool dram_queue_model_enabled):
   m_memory_manager(memory_manager)
{
   m_dram_perf_model = new DramPerfModel(dram_access_cost, 
         dram_bandwidth,
         dram_queue_model_enabled);

   m_dram_access_count = new AccessCountMap[NUM_ACCESS_TYPES];
}

DramCntlr::~DramCntlr()
{
   printDramAccessCount();
   delete [] m_dram_access_count;

   delete m_dram_perf_model;
}

void
DramCntlr::handleMsgFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg)
{
   ShmemMsg::msg_t shmem_msg_type = shmem_msg->getMsgType();

   switch (shmem_msg_type)
   {
   case ShmemMsg::GET_DATA_REQ:
      getDataFromDram(sender, shmem_msg);
      break;

   case ShmemMsg::PUT_DATA_REQ:
      putDataToDram(sender, shmem_msg);
      break;

   default:
      LOG_PRINT_ERROR("Unrecognized Shmem Msg Type(%u)", shmem_msg_type);
      break;
   }
}

void
DramCntlr::getDataFromDram(core_id_t sender, ShmemMsg* shmem_msg)
{
   IntPtr address = shmem_msg->getAddress();
   core_id_t requester = shmem_msg->getRequester();

   assert(shmem_msg->getDataBuf() == NULL);
   assert(shmem_msg->getDataLength() == 0);
   
   Byte data_buf[getCacheBlockSize()];
   
   if (m_data_map[address] == NULL)
   {
      m_data_map[address] = new Byte[getCacheBlockSize()];
      memset((void*) m_data_map[address], 0x00, getCacheBlockSize());
   }
   memcpy((void*) data_buf, (void*) m_data_map[address], getCacheBlockSize());

   UInt64 dram_access_latency = runDramPerfModel(requester);
   getShmemPerfModel()->incrCycleCount(dram_access_latency);

   addToDramAccessCount(address, READ);

   // Send Data back
   getMemoryManager()->sendMsg(ShmemMsg::GET_DATA_REP,
                               MemComponent::DRAM, MemComponent::DRAM_DIR,
                               requester /* requester */,
                               sender /* receiver */,
                               address,
                               false /* reply_expected */,
                               data_buf, getCacheBlockSize());
}

void
DramCntlr::putDataToDram(core_id_t sender, ShmemMsg* shmem_msg)
{
   IntPtr address = shmem_msg->getAddress();
   core_id_t requester = shmem_msg->getRequester();

   assert(shmem_msg->getDataLength() == getCacheBlockSize());
   Byte* data_buf = shmem_msg->getDataBuf();
   
   if (m_data_map[address] == NULL)
   {
      LOG_PRINT_ERROR("Data Buffer does not exist");
   }
   memcpy((void*) m_data_map[address], (void*) data_buf, getCacheBlockSize());

   runDramPerfModel(requester);
   
   addToDramAccessCount(address, WRITE);
}

UInt64
DramCntlr::runDramPerfModel(core_id_t requester)
{
   UInt64 pkt_cycle_count = getShmemPerfModel()->getCycleCount();
   UInt64 pkt_size = (UInt64) getCacheBlockSize();

   volatile float core_frequency = m_memory_manager->getCore()->getPerformanceModel()->getFrequency();
   UInt64 pkt_time = convertCycleCount(pkt_cycle_count, core_frequency, 1.0);

   UInt64 dram_access_latency = m_dram_perf_model->getAccessLatency(pkt_time, pkt_size, requester);
   
   return convertCycleCount(dram_access_latency, 1.0, core_frequency);
}

void
DramCntlr::addToDramAccessCount(IntPtr address, access_t access_type)
{
   m_dram_access_count[access_type][address] = m_dram_access_count[access_type][address] + 1;
}

void
DramCntlr::printDramAccessCount()
{
   for (UInt32 k = 0; k < NUM_ACCESS_TYPES; k++)
   {
      for (AccessCountMap::iterator i = m_dram_access_count[k].begin(); i != m_dram_access_count[k].end(); i++)
      {
         if ((*i).second > 100)
         {
            LOG_PRINT("Dram Cntlr(%i), Address(0x%x), Access Count(%llu), Access Type(%s)", 
                  m_memory_manager->getCore()->getId(), (*i).first, (*i).second,
                  (k == READ)? "READ" : "WRITE");
         }
      }
   }
}

UInt32
DramCntlr::getCacheBlockSize()
{
   return getMemoryManager()->getCacheBlockSize();
}

ShmemPerfModel*
DramCntlr::getShmemPerfModel()
{
   return getMemoryManager()->getShmemPerfModel();
}

}
