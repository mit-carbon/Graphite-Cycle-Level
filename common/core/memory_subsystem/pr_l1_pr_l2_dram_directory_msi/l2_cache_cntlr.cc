#include "l1_cache_cntlr.h"
#include "l2_cache_cntlr.h"
#include "log.h"
#include "memory_manager.h"

namespace PrL1PrL2DramDirectoryMSI
{

L2CacheCntlr::L2CacheCntlr(MemoryManager* memory_manager,
                           L1CacheCntlr* l1_cache_cntlr,
                           AddressHomeLookup* dram_directory_home_lookup,
                           UInt32 cache_block_size,
                           UInt32 l2_cache_size, UInt32 l2_cache_associativity,
                           std::string l2_cache_replacement_policy):
   m_memory_manager(memory_manager),
   m_l1_cache_cntlr(l1_cache_cntlr),
   m_dram_directory_home_lookup(dram_directory_home_lookup)
{
   m_l2_cache = new Cache("L2",
         l2_cache_size, 
         l2_cache_associativity, 
         cache_block_size, 
         l2_cache_replacement_policy, 
         CacheBase::PR_L2_CACHE);
   
   m_l2_cache_contention_model = new QueueModelSimple(false);

   // Register the Event Handler
   if (getCoreId() == 0)
      Event::registerHandler(L2_CACHE_ACCESS_MSG, handleL2CacheAccessMsg);
}

L2CacheCntlr::~L2CacheCntlr()
{
   // Unregister the Event Handler
   if (getCoreId() == 0)
      Event::unregisterHandler(L2_CACHE_ACCESS_MSG);

   delete m_l2_cache_contention_model;
   delete m_l2_cache;
}

PrL2CacheBlockInfo*
L2CacheCntlr::getCacheBlockInfo(IntPtr address)
{
   return (PrL2CacheBlockInfo*) m_l2_cache->peekSingleLine(address);
}

CacheState::cstate_t
L2CacheCntlr::getCacheState(PrL2CacheBlockInfo* l2_cache_block_info)
{
   return (l2_cache_block_info == NULL) ? CacheState::INVALID : l2_cache_block_info->getCState();
}

void
L2CacheCntlr::setCacheState(PrL2CacheBlockInfo* l2_cache_block_info, CacheState::cstate_t cstate)
{
   assert(l2_cache_block_info);
   l2_cache_block_info->setCState(cstate);
}

void
L2CacheCntlr::invalidateCacheBlock(IntPtr address)
{
   m_l2_cache->invalidateSingleLine(address);
}

void
L2CacheCntlr::retrieveCacheBlock(IntPtr address, Byte* data_buf)
{
   __attribute(__unused__) PrL2CacheBlockInfo* l2_cache_block_info = (PrL2CacheBlockInfo*) m_l2_cache->accessSingleLine(address, Cache::LOAD, data_buf, getCacheBlockSize());
   assert(l2_cache_block_info);
}

void
L2CacheCntlr::writeCacheBlock(IntPtr address, UInt32 offset, Byte* data_buf, UInt32 data_length)
{
   __attribute(__unused__) PrL2CacheBlockInfo* l2_cache_block_info = (PrL2CacheBlockInfo*) m_l2_cache->accessSingleLine(address + offset, Cache::STORE, data_buf, data_length);
   assert(l2_cache_block_info);
}

PrL2CacheBlockInfo*
L2CacheCntlr::insertCacheBlock(IntPtr address, CacheState::cstate_t cstate, Byte* data_buf)
{
   bool eviction;
   IntPtr evict_address;
   PrL2CacheBlockInfo evict_block_info;
   Byte evict_buf[getCacheBlockSize()];

   m_l2_cache->insertSingleLine(address, data_buf,
         &eviction, &evict_address, &evict_block_info, evict_buf);
   PrL2CacheBlockInfo* l2_cache_block_info = getCacheBlockInfo(address);
   setCacheState(l2_cache_block_info, cstate);

   if (eviction)
   {
      LOG_PRINT("Eviction: addr(0x%x)", evict_address);
      invalidateCacheBlockInL1(evict_block_info.getCachedLoc(), evict_address);

      UInt32 home_node_id = getHome(evict_address);
      if (evict_block_info.getCState() == CacheState::MODIFIED)
      {
         // Send back the data also
         getMemoryManager()->sendMsg(ShmemMsg::FLUSH_REP, 
               MemComponent::L2_CACHE, MemComponent::DRAM_DIR, 
               getCoreId() /* requester */, 
               home_node_id /* receiver */, 
               evict_address, 
               evict_buf, getCacheBlockSize());
      }
      else
      {
         LOG_ASSERT_ERROR(evict_block_info.getCState() == CacheState::SHARED,
               "evict_address(0x%x), evict_state(%u), cached_loc(%u)",
               evict_address, evict_block_info.getCState(), evict_block_info.getCachedLoc());
         getMemoryManager()->sendMsg(ShmemMsg::INV_REP, 
               MemComponent::L2_CACHE, MemComponent::DRAM_DIR, 
               getCoreId() /* requester */, 
               home_node_id /* receiver */, 
               evict_address);
      }
   }

   return l2_cache_block_info;
}

void
L2CacheCntlr::setCacheStateInL1(MemComponent::component_t mem_component, IntPtr address, CacheState::cstate_t cstate)
{
   if (mem_component != MemComponent::INVALID_MEM_COMPONENT)
      m_l1_cache_cntlr->setCacheState(mem_component, address, cstate);
}

void
L2CacheCntlr::invalidateCacheBlockInL1(MemComponent::component_t mem_component, IntPtr address)
{
   if (mem_component != MemComponent::INVALID_MEM_COMPONENT)
      m_l1_cache_cntlr->invalidateCacheBlock(mem_component, address);
}

void
L2CacheCntlr::insertCacheBlockInL1(MemComponent::component_t mem_component,
      IntPtr address, PrL2CacheBlockInfo* l2_cache_block_info,
      CacheState::cstate_t cstate, Byte* data_buf)
{
   bool eviction;
   IntPtr evict_address;

   // Insert the Cache Block in L1 Cache
   m_l1_cache_cntlr->insertCacheBlock(mem_component, address, cstate, data_buf, &eviction, &evict_address);

   // Set the Present bit in L2 Cache corresponding to the inserted block
   l2_cache_block_info->setCachedLoc(mem_component);

   if (eviction)
   {
      // Clear the Present bit in L2 Cache corresponding to the evicted block
      PrL2CacheBlockInfo* evict_block_info = getCacheBlockInfo(evict_address);
      evict_block_info->clearCachedLoc(mem_component);
   }
}

bool
L2CacheCntlr::processShmemReqFromL1Cache(MemComponent::component_t req_mem_component, ShmemMsg::msg_t msg_type, IntPtr address, bool modeled)
{
   PrL2CacheBlockInfo* l2_cache_block_info = getCacheBlockInfo(address);
   CacheState::cstate_t cstate = getCacheState(l2_cache_block_info);

   bool shmem_req_ends_in_l2_cache = shmemReqEndsInL2Cache(msg_type, cstate, modeled);
   if (shmem_req_ends_in_l2_cache)
   {
      Byte data_buf[getCacheBlockSize()];
      retrieveCacheBlock(address, data_buf);

      insertCacheBlockInL1(req_mem_component, address, l2_cache_block_info, cstate, data_buf);
   }
   
   return shmem_req_ends_in_l2_cache;
}

void
handleL2CacheAccessMsg(Event* event)
{
   UnstructuredBuffer* event_args = event->getArgs();
   
   L2CacheCntlr* l2_cache_cntlr;
   core_id_t sender;
   ShmemMsg shmem_msg;
   (*event_args) >> l2_cache_cntlr >> sender >> shmem_msg;

   // Set the Time correctly
   l2_cache_cntlr->getShmemPerfModel()->setCycleCount(event->getTime());

   switch (shmem_msg.getSenderMemComponent())
   {
   case MemComponent::L1_ICACHE:
   case MemComponent::L1_DCACHE:
      l2_cache_cntlr->__handleMsgFromL1Cache(&shmem_msg);
      break;
   
   case MemComponent::DRAM_DIR:
      l2_cache_cntlr->__handleMsgFromDramDirectory(sender, &shmem_msg);
      break;

   default:
      LOG_PRINT_ERROR("Unrecognized sender component(%u)", shmem_msg.getSenderMemComponent());
      break;
   }
}

void
L2CacheCntlr::scheduleNextPendingRequest(UInt64 time)
{
   LOG_PRINT("scheduleNextPendingRequest(Time[%llu], Request Queue Size[%u])", \
         time, m_pending_dram_directory_req_list.size());

   if (!m_pending_dram_directory_req_list.empty())
   {
      // Get the L2 Cache Access Req at the front of the list
      pair<core_id_t,ShmemMsg*> blocked_req = m_pending_dram_directory_req_list.front();
      m_pending_dram_directory_req_list.pop_front();
      
      core_id_t sender = blocked_req.first;
      ShmemMsg* shmem_msg = blocked_req.second;

      scheduleRequest(time, sender, shmem_msg);
      
      delete shmem_msg;
   }
}

void
L2CacheCntlr::scheduleRequest(UInt64 time, core_id_t sender, ShmemMsg* shmem_msg)
{
   LOG_PRINT("scheduleRequest(Time[%llu], Sender[%i], Address[0x%llx], Type[%u], Requester[%i])", \
         time, sender, shmem_msg->getAddress(), shmem_msg->getMsgType(), shmem_msg->getRequester());

   // Calculate the L2 Cache Contention Delay
   // Assume that the L2 Cache can process one request per cycle
   UInt64 queue_delay = m_l2_cache_contention_model->computeQueueDelay(time, 1);
   LOG_PRINT("L2 Cache Contention Delay(%llu)", queue_delay);
   time += queue_delay;

   // Push an event to access the L2 Cache from the Directory
   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) << this << sender << (*shmem_msg);
   Event* event = new Event((Event::Type) L2_CACHE_ACCESS_MSG, time, event_args);
   Event::processInOrder(event, getCoreId(), EventQueue::ORDERED);
}

void
L2CacheCntlr::handleMsgFromL1Cache(ShmemMsg* shmem_msg)
{
   assert(!isLocked());

   // Schedule the request according to the processing delays within the L2 Cache
   scheduleRequest(getShmemPerfModel()->getCycleCount(), getCoreId(), shmem_msg);
}

void
L2CacheCntlr::__handleMsgFromL1Cache(ShmemMsg* shmem_msg)
{
   assert(!isLocked());

   // Update to account for time
   getMemoryManager()->incrCycleCount(MemComponent::L2_CACHE, CachePerfModel::ACCESS_CACHE_DATA_AND_TAGS);

   IntPtr address = shmem_msg->getAddress();
   ShmemMsg::msg_t shmem_msg_type = shmem_msg->getMsgType();
   MemComponent::component_t sender_mem_component = shmem_msg->getSenderMemComponent();

   assert(shmem_msg->getDataBuf() == NULL);
   assert(shmem_msg->getDataLength() == 0);

   L2MissStatus* l2_miss_status = new L2MissStatus(address, sender_mem_component);
   m_miss_status_map.insert(l2_miss_status);
   LOG_PRINT("L2: Inserted Miss Status [Address(0x%llx), Mem Component(%u)]", address, sender_mem_component);
   
   switch (shmem_msg_type)
   {
      case ShmemMsg::EX_REQ:
         processExReqFromL1Cache(shmem_msg);
         break;

      case ShmemMsg::SH_REQ:
         processShReqFromL1Cache(shmem_msg);
         break;

      default:
         LOG_PRINT_ERROR("Unrecognized shmem msg type (%u)", shmem_msg_type);
         break;
   }
}

void
L2CacheCntlr::processExReqFromL1Cache(ShmemMsg* shmem_msg)
{
   // We need to send a request to the Dram Directory Cache
   IntPtr address = shmem_msg->getAddress();

   PrL2CacheBlockInfo* l2_cache_block_info = getCacheBlockInfo(address);
   CacheState::cstate_t cstate = getCacheState(l2_cache_block_info);

   assert((cstate == CacheState::INVALID) || (cstate == CacheState::SHARED));
   if (cstate == CacheState::SHARED)
   {
      // This will clear the 'Present' bit also
      invalidateCacheBlock(address);
      getMemoryManager()->sendMsg(ShmemMsg::INV_REP, 
            MemComponent::L2_CACHE, MemComponent::DRAM_DIR, 
            getCoreId() /* requester */, 
            getHome(address) /* receiver */, 
            address);
   }

   getMemoryManager()->sendMsg(ShmemMsg::EX_REQ, 
         MemComponent::L2_CACHE, MemComponent::DRAM_DIR, 
         getCoreId() /* requester */, 
         getHome(address) /* receiver */, 
         address);
}

void
L2CacheCntlr::processShReqFromL1Cache(ShmemMsg* shmem_msg)
{
   IntPtr address = shmem_msg->getAddress();

   getMemoryManager()->sendMsg(ShmemMsg::SH_REQ, 
         MemComponent::L2_CACHE, MemComponent::DRAM_DIR, 
         getCoreId() /* requester */, 
         getHome(address) /* receiver */, 
         address);
}


void
L2CacheCntlr::handleMsgFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg)
{
   LOG_PRINT("handleMsgFromDramDirectory(Sender[%i], Address[0x%llx], Type[%u], Requester[%i])", \
         sender, shmem_msg->getAddress(), shmem_msg->getMsgType(), shmem_msg->getRequester());

   if ( (isLocked()) || (!m_pending_dram_directory_req_list.empty()) )
   {
      LOG_PRINT("Locked. Cloning Message and pushing it to the waiting list");
      // Clone the packet and queue it up
      ShmemMsg* cloned_shmem_msg = new ShmemMsg(shmem_msg);
      m_pending_dram_directory_req_list.push_back(make_pair(sender,cloned_shmem_msg));
   }
   else // If (NOT locked) AND (NO earlier requests pending)
   {
      // Schedule the request according to the processing delays within the L2 Cache
      scheduleRequest(getShmemPerfModel()->getCycleCount(), sender, shmem_msg);
   }
}

// Called after going through the contention model
void
L2CacheCntlr::__handleMsgFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg)
{
   LOG_PRINT("__handleMsgFromDramDirectory(Sender[%i], Address[0x%llx], Type[%u], Requester[%i])", \
         sender, shmem_msg->getAddress(), shmem_msg->getMsgType(), shmem_msg->getRequester());

   assert(!isLocked());

   // Schedule the next request from the dram directory (if any)
   scheduleNextPendingRequest(getShmemPerfModel()->getCycleCount());

   // Account for access time
   getMemoryManager()->incrCycleCount(MemComponent::L2_CACHE, CachePerfModel::ACCESS_CACHE_DATA_AND_TAGS);
   
   ShmemMsg::msg_t shmem_msg_type = shmem_msg->getMsgType();
   IntPtr address = shmem_msg->getAddress();

   switch (shmem_msg_type)
   {
      case ShmemMsg::EX_REP:
         processExRepFromDramDirectory(sender, shmem_msg);
         break;
      case ShmemMsg::SH_REP:
         processShRepFromDramDirectory(sender, shmem_msg);
         break;
      case ShmemMsg::INV_REQ:
         processInvReqFromDramDirectory(sender, shmem_msg);
         break;
      case ShmemMsg::FLUSH_REQ:
         processFlushReqFromDramDirectory(sender, shmem_msg);
         break;
      case ShmemMsg::WB_REQ:
         processWbReqFromDramDirectory(sender, shmem_msg);
         break;
      default:
         LOG_PRINT_ERROR("Unrecognized msg type(%u)", shmem_msg_type);
         break;
   }

   if ((shmem_msg_type == ShmemMsg::EX_REP) || (shmem_msg_type == ShmemMsg::SH_REP))
   {
      // Remove the Miss Status Information
      L2MissStatus* l2_miss_status = (L2MissStatus*) m_miss_status_map.get(address);
      m_miss_status_map.erase(l2_miss_status);
      
      // Signal the L1 Cache that data is ready
      LOG_PRINT("Signal the L1 Cache Cntlr that the data is ready");
      m_l1_cache_cntlr->signalDataReady(l2_miss_status->_mem_component, address);
      
      delete l2_miss_status;
      LOG_PRINT("Erased L2MissStatus data structure from the map");
   }
}

void
L2CacheCntlr::processExRepFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg)
{
   IntPtr address = shmem_msg->getAddress();
   Byte* data_buf = shmem_msg->getDataBuf();

   PrL2CacheBlockInfo* l2_cache_block_info = insertCacheBlock(address, CacheState::MODIFIED, data_buf);
   LOG_PRINT("Inserted Cache Block into L2 Cache");

   // Insert Cache Block in L1 Cache
   // Support for non-blocking caches can be added in this way
   L2MissStatus* l2_miss_status = (L2MissStatus*) m_miss_status_map.get(address);
   MemComponent::component_t mem_component = l2_miss_status->_mem_component;
   assert (mem_component == MemComponent::L1_DCACHE);
   
   insertCacheBlockInL1(mem_component, address, l2_cache_block_info, CacheState::MODIFIED, data_buf);
   LOG_PRINT("Inserted Cache Block into L1 Cache(%u)", mem_component);
}

void
L2CacheCntlr::processShRepFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg)
{
   IntPtr address = shmem_msg->getAddress();
   Byte* data_buf = shmem_msg->getDataBuf();

   // Insert Cache Block in L2 Cache
   PrL2CacheBlockInfo* l2_cache_block_info = insertCacheBlock(address, CacheState::SHARED, data_buf);
   LOG_PRINT("Inserted Cache Block into L2 Cache");

   // Insert Cache Block in L1 Cache
   // Support for non-blocking caches can be added in this way
   L2MissStatus* l2_miss_status = (L2MissStatus*) m_miss_status_map.get(address);
   MemComponent::component_t mem_component = l2_miss_status->_mem_component;
   
   insertCacheBlockInL1(mem_component, address, l2_cache_block_info, CacheState::SHARED, data_buf);
   LOG_PRINT("Inserted Cache Block into L1 Cache(%u)", mem_component);
}

void
L2CacheCntlr::processInvReqFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg)
{
   IntPtr address = shmem_msg->getAddress();

   PrL2CacheBlockInfo* l2_cache_block_info = getCacheBlockInfo(address);
   CacheState::cstate_t cstate = getCacheState(l2_cache_block_info);
   if (cstate != CacheState::INVALID)
   {
      assert(cstate == CacheState::SHARED);
  
      // Invalidate the line in L1 Cache
      invalidateCacheBlockInL1(l2_cache_block_info->getCachedLoc(), address);
      // Invalidate the line in the L2 cache also
      invalidateCacheBlock(address);

      getMemoryManager()->sendMsg(ShmemMsg::INV_REP, 
            MemComponent::L2_CACHE, MemComponent::DRAM_DIR, 
            shmem_msg->getRequester() /* requester */, 
            sender /* receiver */, 
            address);
   }
}

void
L2CacheCntlr::processFlushReqFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg)
{
   IntPtr address = shmem_msg->getAddress();

   PrL2CacheBlockInfo* l2_cache_block_info = getCacheBlockInfo(address);
   CacheState::cstate_t cstate = getCacheState(l2_cache_block_info);
   if (cstate != CacheState::INVALID)
   {
      assert(cstate == CacheState::MODIFIED);
      
      // Invalidate the line in L1 Cache
      invalidateCacheBlockInL1(l2_cache_block_info->getCachedLoc(), address);

      // Flush the line
      Byte data_buf[getCacheBlockSize()];
      retrieveCacheBlock(address, data_buf);
      invalidateCacheBlock(address);

      getMemoryManager()->sendMsg(ShmemMsg::FLUSH_REP, 
            MemComponent::L2_CACHE, MemComponent::DRAM_DIR, 
            shmem_msg->getRequester() /* requester */, 
            sender /* receiver */, 
            address, 
            data_buf, getCacheBlockSize());
   }
}

void
L2CacheCntlr::processWbReqFromDramDirectory(core_id_t sender, ShmemMsg* shmem_msg)
{
   IntPtr address = shmem_msg->getAddress();

   PrL2CacheBlockInfo* l2_cache_block_info = getCacheBlockInfo(address);
   CacheState::cstate_t cstate = getCacheState(l2_cache_block_info);
   if (cstate != CacheState::INVALID)
   {
      assert(cstate == CacheState::MODIFIED);
 
      // Set the Appropriate Cache State in L1 also
      setCacheStateInL1(l2_cache_block_info->getCachedLoc(), address, CacheState::SHARED);

      // Write-Back the line
      Byte data_buf[getCacheBlockSize()];
      retrieveCacheBlock(address, data_buf);
      setCacheState(l2_cache_block_info, CacheState::SHARED);

      getMemoryManager()->sendMsg(ShmemMsg::WB_REP, 
            MemComponent::L2_CACHE, MemComponent::DRAM_DIR, 
            shmem_msg->getRequester() /* requester */, 
            sender /* receiver */, 
            address,
            data_buf, getCacheBlockSize());
   }
}

bool
L2CacheCntlr::shmemReqEndsInL2Cache(ShmemMsg::msg_t shmem_msg_type, CacheState::cstate_t cstate, bool modeled)
{
   bool cache_hit = false;

   switch (shmem_msg_type)
   {
   case ShmemMsg::EX_REQ:
      cache_hit = CacheState(cstate).writable();
      break;

   case ShmemMsg::SH_REQ:
      cache_hit = CacheState(cstate).readable();
      break;

   default:
      LOG_PRINT_ERROR("Unsupported Shmem Msg Type: %u", shmem_msg_type);
      break;
   }

   if (modeled)
   {
      m_l2_cache->updateCounters(cache_hit);
   }

   return cache_hit;
}

core_id_t
L2CacheCntlr::getCoreId()
{
   return getMemoryManager()->getCore()->getId();
}

UInt32
L2CacheCntlr::getCacheBlockSize()
{
   return getMemoryManager()->getCacheBlockSize();
}

ShmemPerfModel*
L2CacheCntlr::getShmemPerfModel()
{
   return getMemoryManager()->getShmemPerfModel();
}

bool
L2CacheCntlr::isLocked()
{
   return m_l1_cache_cntlr->isLocked();
}

}
