using namespace std;

#include "dram_directory_cntlr.h"
#include "log.h"
#include "memory_manager.h"

namespace PrL1PrL2DramDirectoryMSI
{

DramDirectoryCntlr::DramDirectoryCntlr(MemoryManager* memory_manager,
                                       UInt32 dram_directory_total_entries,
                                       UInt32 dram_directory_associativity,
                                       UInt32 cache_block_size,
                                       UInt32 dram_directory_max_num_sharers,
                                       UInt32 dram_directory_max_hw_sharers,
                                       string dram_directory_type_str,
                                       UInt64 dram_directory_cache_access_delay_in_ns,
                                       UInt32 num_dram_cntlrs):
   m_memory_manager(memory_manager)
{
   LOG_PRINT("Dram Directory Cntlr ctor");

   m_dram_directory_cache = new DramDirectoryCache(
         memory_manager,
         dram_directory_type_str,
         dram_directory_total_entries,
         dram_directory_associativity,
         cache_block_size,
         dram_directory_max_hw_sharers,
         dram_directory_max_num_sharers,
         num_dram_cntlrs,
         dram_directory_cache_access_delay_in_ns);
   LOG_PRINT("Instantiated Dram Directory Cache");

   m_dram_directory_req_queue_list = new ReqQueueList();
   LOG_PRINT("Instantiated Dram Directory Req Queues");

   // Contention Model
   m_dram_directory_contention_model = new QueueModelSimple();
}

DramDirectoryCntlr::~DramDirectoryCntlr()
{
   delete m_dram_directory_contention_model;

   delete m_dram_directory_cache;
   delete m_dram_directory_req_queue_list;
}

void
DramDirectoryCntlr::registerEventHandlers()
{
   Event::registerHandler(DRAM_DIRECTORY_ACCESS_REQ, handleDramDirectoryAccessReq);
   Event::registerHandler(DRAM_DIRECTORY_SCHEDULE_NEXT_REQ_FROM_L2_CACHE, scheduleNextDramDirectoryAccessReqFromL2Cache);
   Event::registerHandler(DRAM_DIRECTORY_HANDLE_NEXT_REQ_FROM_L2_CACHE, handleNextDramDirectoryAccessReqFromL2Cache);
}

void
DramDirectoryCntlr::unregisterEventHandlers()
{
   Event::unregisterHandler(DRAM_DIRECTORY_HANDLE_NEXT_REQ_FROM_L2_CACHE);
   Event::unregisterHandler(DRAM_DIRECTORY_SCHEDULE_NEXT_REQ_FROM_L2_CACHE);
   Event::unregisterHandler(DRAM_DIRECTORY_ACCESS_REQ);
}

void
scheduleNextDramDirectoryAccessReqFromL2Cache(Event* event)
{
   UnstructuredBuffer* event_args = event->getArgs();

   DramDirectoryCntlr* dram_directory_cntlr;
   IntPtr address;
   (*event_args) >> dram_directory_cntlr >> address;

   // Set the Time Correctly
   dram_directory_cntlr->getShmemPerfModel()->setCycleCount(event->getTime());

   // Call the Function
   dram_directory_cntlr->__scheduleNextReqFromL2Cache(address);
}

void
handleNextDramDirectoryAccessReqFromL2Cache(Event* event)
{
   UnstructuredBuffer* event_args = event->getArgs();

   DramDirectoryCntlr* dram_directory_cntlr;
   IntPtr address;
   (*event_args) >> dram_directory_cntlr >> address;

   // Set the Time Correctly
   dram_directory_cntlr->getShmemPerfModel()->setCycleCount(event->getTime());

   // Call the Function
   dram_directory_cntlr->handleNextReqFromL2Cache(address);
}

void
DramDirectoryCntlr::scheduleNextReqFromL2Cache(IntPtr address)
{
   assert(!m_dram_directory_req_queue_list->empty(address));
   ShmemReq* completed_shmem_req = m_dram_directory_req_queue_list->dequeue(address);
   delete completed_shmem_req;

   if (!m_dram_directory_req_queue_list->empty(address))
   {
      // Add this address to the inactive address set
      assert(m_inactive_address_set.find(address) == m_inactive_address_set.end());
      m_inactive_address_set.insert(address);

      LOG_PRINT("A new shmem req for address(0x%x) found", address);
      
      UInt64 time = getShmemPerfModel()->getCycleCount();
      LOG_PRINT("scheduleNextReqFromL2Cache(Time[%llu], Address[0x%llx])", time, address);

      UnstructuredBuffer* event_args = new UnstructuredBuffer();
      (*event_args) << this << address;
      Event* event = new Event((Event::Type) DRAM_DIRECTORY_SCHEDULE_NEXT_REQ_FROM_L2_CACHE, time, event_args);
      Event::processInOrder(event, getCoreId(), EventQueue::ORDERED);
   }
}

void
DramDirectoryCntlr::__scheduleNextReqFromL2Cache(IntPtr address)
{
   assert(!m_dram_directory_req_queue_list->empty(address));
   UInt64 time = getShmemPerfModel()->getCycleCount();

   // Calculate the Dram Directory Contention Delay
   // Assume that the Dram Directory can process one request per cycle
   UInt64 queue_delay = m_dram_directory_contention_model->computeQueueDelay(time, 1);
   LOG_PRINT("Contention Delay(%llu)", queue_delay);
   time += queue_delay;

   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) << this << address;
   Event* event = new Event((Event::Type) DRAM_DIRECTORY_HANDLE_NEXT_REQ_FROM_L2_CACHE, time, event_args);
   Event::processInOrder(event, getCoreId(), EventQueue::ORDERED);
}

void
DramDirectoryCntlr::handleNextReqFromL2Cache(IntPtr address)
{
   LOG_PRINT("Start handleNextReqFromL2Cache(0x%llx)", address);
   assert (!m_dram_directory_req_queue_list->empty(address));

   // Remove this address from the inactive address set
   assert (m_inactive_address_set.find(address) != m_inactive_address_set.end());
   m_inactive_address_set.erase(address);
   
   ShmemReq* shmem_req = m_dram_directory_req_queue_list->front(address);

   // Account for Dram Directory Cache access delay
   assert(getShmemPerfModel());
   getShmemPerfModel()->incrCycleCount(getDramDirectoryCache()->getAccessDelay());

   // Update the Shared Mem Cycle Counts appropriately
   shmem_req->setTime(getShmemPerfModel()->getCycleCount());

   switch (shmem_req->getShmemMsg()->getMsgType())
   {
   case ShmemMsg::EX_REQ:
      processExReqFromL2Cache(shmem_req);
      break;

   case ShmemMsg::SH_REQ:
      processShReqFromL2Cache(shmem_req);
      break;
   
   default:
      LOG_PRINT_ERROR("Unrecognized Request(%u)", shmem_req->getShmemMsg()->getMsgType());
      break;
   }
   LOG_PRINT("End handleNextReqFromL2Cache(0x%llx)", address);
}

void
handleDramDirectoryAccessReq(Event* event)
{
   UnstructuredBuffer* event_args = event->getArgs();
   
   DramDirectoryCntlr* dram_directory_cntlr;
   core_id_t sender;
   ShmemMsg shmem_msg;
   (*event_args) >> dram_directory_cntlr >> sender >> shmem_msg;

   // Set the Time correctly
   dram_directory_cntlr->getShmemPerfModel()->setCycleCount(event->getTime());

   switch (shmem_msg.getSenderMemComponent())
   {
   case MemComponent::L2_CACHE:
      dram_directory_cntlr->__handleMsgFromL2Cache(sender, &shmem_msg);
      break;

   case MemComponent::DRAM:
      dram_directory_cntlr->__handleMsgFromDram(sender, &shmem_msg);
      break;

   default:
      LOG_PRINT_ERROR("Unrecognized sender component(%u)", shmem_msg.getSenderMemComponent());
      break;
   }
}

void
DramDirectoryCntlr::scheduleRequest(core_id_t sender, ShmemMsg* shmem_msg)
{
   UInt64 time = getShmemPerfModel()->getCycleCount();
   LOG_PRINT("scheduleRequest(Time[%llu], Sender[%i], Address[0x%llx], Type[%u], Requester[%i])", \
         time, sender, shmem_msg->getAddress(), shmem_msg->getMsgType(), shmem_msg->getRequester());

   // Calculate the L2 Cache Contention Delay
   // Assume that the L2 Cache can process one request per cycle
   UInt64 queue_delay = m_dram_directory_contention_model->computeQueueDelay(time, 1);
   LOG_PRINT("Contention Delay(%llu)", queue_delay);
   time += queue_delay;

   // Push an event to access the L2 Cache from the Directory
   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) << this << sender << (*shmem_msg);
   Event* event = new Event((Event::Type) DRAM_DIRECTORY_ACCESS_REQ, time, event_args);
   Event::processInOrder(event, getCoreId(), EventQueue::ORDERED);
}

void
DramDirectoryCntlr::handleMsgFromL2Cache(core_id_t sender, ShmemMsg* shmem_msg)
{
   scheduleRequest(sender, shmem_msg);
}

void
DramDirectoryCntlr::__handleMsgFromL2Cache(core_id_t sender, ShmemMsg* shmem_msg)
{
   // Account for Dram Directory Cache access delay
   assert(getShmemPerfModel());
   getShmemPerfModel()->incrCycleCount(getDramDirectoryCache()->getAccessDelay());

   ShmemMsg::msg_t shmem_msg_type = shmem_msg->getMsgType();
   UInt64 msg_time = getShmemPerfModel()->getCycleCount();

   switch (shmem_msg_type)
   {
   case ShmemMsg::EX_REQ:
   case ShmemMsg::SH_REQ:

      {
         IntPtr address = shmem_msg->getAddress();
         
         // Add request onto a queue
         ShmemReq* shmem_req = new ShmemReq(shmem_msg, msg_time);
         m_dram_directory_req_queue_list->enqueue(address, shmem_req);
         if (m_dram_directory_req_queue_list->size(address) == 1)
         {
            if (shmem_msg_type == ShmemMsg::EX_REQ)
               processExReqFromL2Cache(shmem_req);
            else if (shmem_msg_type == ShmemMsg::SH_REQ)
               processShReqFromL2Cache(shmem_req);
            else
               LOG_PRINT_ERROR("Unrecognized Request(%u)", shmem_msg_type);
         }
      }
      break;

   case ShmemMsg::INV_REP:
      processInvRepFromL2Cache(sender, shmem_msg);
      break;

   case ShmemMsg::FLUSH_REP:
      processFlushRepFromL2Cache(sender, shmem_msg);
      break;

   case ShmemMsg::WB_REP:
      processWbRepFromL2Cache(sender, shmem_msg);
      break;

   default:
      LOG_PRINT_ERROR("Unrecognized Shmem Msg Type: %u", shmem_msg_type);
      break;
   }
}

DirectoryEntry*
DramDirectoryCntlr::processDirectoryEntryAllocationReq(ShmemReq* shmem_req)
{
   IntPtr address = shmem_req->getShmemMsg()->getAddress();
   core_id_t requester = shmem_req->getShmemMsg()->getRequester();
   UInt64 msg_time = getShmemPerfModel()->getCycleCount();

   std::vector<DirectoryEntry*> replacement_candidate_list;
   m_dram_directory_cache->getReplacementCandidates(address, replacement_candidate_list);

   std::vector<DirectoryEntry*>::iterator it;
   std::vector<DirectoryEntry*>::iterator replacement_candidate = replacement_candidate_list.end();
   for (it = replacement_candidate_list.begin(); it != replacement_candidate_list.end(); it++)
   {
      if ( ( (replacement_candidate == replacement_candidate_list.end()) ||
             ((*replacement_candidate)->getNumSharers() > (*it)->getNumSharers()) 
           )
           &&
           (m_dram_directory_req_queue_list->size((*it)->getAddress()) == 0)
         )
      {
         replacement_candidate = it;
      }
   }

   LOG_ASSERT_ERROR(replacement_candidate != replacement_candidate_list.end(),
         "Cant find a directory entry to be replaced with a non-zero request list");

   IntPtr replaced_address = (*replacement_candidate)->getAddress();

   // We get the entry with the lowest number of sharers
   DirectoryEntry* directory_entry = m_dram_directory_cache->replaceDirectoryEntry(replaced_address, address);

   ShmemMsg nullify_msg(ShmemMsg::NULLIFY_REQ, MemComponent::DRAM_DIR, MemComponent::DRAM_DIR, requester, replaced_address, NULL, 0);

   ShmemReq* nullify_req = new ShmemReq(&nullify_msg, msg_time);
   m_dram_directory_req_queue_list->enqueue(replaced_address, nullify_req);

   assert(m_dram_directory_req_queue_list->size(replaced_address) == 1);
   processNullifyReq(nullify_req);

   return directory_entry;
}

void
DramDirectoryCntlr::processNullifyReq(ShmemReq* shmem_req)
{
   IntPtr address = shmem_req->getShmemMsg()->getAddress();
   core_id_t requester = shmem_req->getShmemMsg()->getRequester();
   
   DirectoryEntry* directory_entry = m_dram_directory_cache->getDirectoryEntry(address);
   assert(directory_entry);

   DirectoryBlockInfo* directory_block_info = directory_entry->getDirectoryBlockInfo();
   DirectoryState::dstate_t curr_dstate = directory_block_info->getDState();

   switch (curr_dstate)
   {
      case DirectoryState::MODIFIED:
         getMemoryManager()->sendMsg(ShmemMsg::FLUSH_REQ, 
               MemComponent::DRAM_DIR, MemComponent::L2_CACHE, 
               requester /* requester */, 
               directory_entry->getOwner() /* receiver */, 
               address);
         break;
   
      case DirectoryState::SHARED:

         {
            pair<bool, vector<SInt32> > sharers_list_pair = directory_entry->getSharersList();
            if (sharers_list_pair.first == true)
            {
               // Broadcast Invalidation Request to all cores 
               // (irrespective of whether they are sharers or not)
               getMemoryManager()->broadcastMsg(ShmemMsg::INV_REQ, 
                     MemComponent::DRAM_DIR, MemComponent::L2_CACHE, 
                     requester /* requester */, 
                     address);
            }
            else
            {
               // Send Invalidation Request to only a specific set of sharers
               for (UInt32 i = 0; i < sharers_list_pair.second.size(); i++)
               {
                  getMemoryManager()->sendMsg(ShmemMsg::INV_REQ, 
                        MemComponent::DRAM_DIR, MemComponent::L2_CACHE, 
                        requester /* requester */, 
                        sharers_list_pair.second[i] /* receiver */, 
                        address);
               }
            }
         }
         break;
   
      case DirectoryState::UNCACHED:

         {
            m_dram_directory_cache->invalidateDirectoryEntry(address);
      
            // Schedule Next Request
            scheduleNextReqFromL2Cache(address);
         }
         break;
   
      default:
         LOG_PRINT_ERROR("Unsupported Directory State: %u", curr_dstate);
         break;
   }

}

void
DramDirectoryCntlr::processExReqFromL2Cache(ShmemReq* shmem_req, Byte* cached_data_buf)
{
   IntPtr address = shmem_req->getShmemMsg()->getAddress();
   core_id_t requester = shmem_req->getShmemMsg()->getRequester();
   
   DirectoryEntry* directory_entry = m_dram_directory_cache->getDirectoryEntry(address);
   if (directory_entry == NULL)
   {
      directory_entry = processDirectoryEntryAllocationReq(shmem_req);
   }

   DirectoryBlockInfo* directory_block_info = directory_entry->getDirectoryBlockInfo();
   DirectoryState::dstate_t curr_dstate = directory_block_info->getDState();

   switch (curr_dstate)
   {
      case DirectoryState::MODIFIED:
         assert(cached_data_buf == NULL);
         getMemoryManager()->sendMsg(ShmemMsg::FLUSH_REQ, 
               MemComponent::DRAM_DIR, MemComponent::L2_CACHE, 
               requester /* requester */, 
               directory_entry->getOwner() /* receiver */, 
               address);
         break;
   
      case DirectoryState::SHARED:

         {
            assert(cached_data_buf == NULL);
            pair<bool, vector<SInt32> > sharers_list_pair = directory_entry->getSharersList();
            if (sharers_list_pair.first == true)
            {
               // Broadcast Invalidation Request to all cores 
               // (irrespective of whether they are sharers or not)
               getMemoryManager()->broadcastMsg(ShmemMsg::INV_REQ, 
                     MemComponent::DRAM_DIR, MemComponent::L2_CACHE, 
                     requester /* requester */, 
                     address);
            }
            else
            {
               // Send Invalidation Request to only a specific set of sharers
               for (UInt32 i = 0; i < sharers_list_pair.second.size(); i++)
               {
                  getMemoryManager()->sendMsg(ShmemMsg::INV_REQ, 
                        MemComponent::DRAM_DIR, MemComponent::L2_CACHE, 
                        requester /* requester */, 
                        sharers_list_pair.second[i] /* receiver */, 
                        address);
               }
            }
         }
         break;
   
      case DirectoryState::UNCACHED:

         {
            if (cached_data_buf)
            {
               // Modifiy the directory entry contents
               bool add_result = directory_entry->addSharer(requester);
               assert(add_result == true);
               directory_entry->setOwner(requester);
               directory_block_info->setDState(DirectoryState::MODIFIED);

               // Send Data Back to the L2 Cache
               retrieveDataAndSendToL2Cache(ShmemMsg::EX_REP, requester, address, cached_data_buf);
               
               // Process Next Request
               scheduleNextReqFromL2Cache(address);
            }

            else
            {
               // Get Data From Dram
               getDataFromDram(address, requester);
            }
         }
         break;
   
      default:
         LOG_PRINT_ERROR("Unsupported Directory State: %u", curr_dstate);
         break;
   }
}

void
DramDirectoryCntlr::processShReqFromL2Cache(ShmemReq* shmem_req, Byte* cached_data_buf)
{
   IntPtr address = shmem_req->getShmemMsg()->getAddress();
   core_id_t requester = shmem_req->getShmemMsg()->getRequester();

   DirectoryEntry* directory_entry = m_dram_directory_cache->getDirectoryEntry(address);
   if (directory_entry == NULL)
   {
      directory_entry = processDirectoryEntryAllocationReq(shmem_req);
   }

   DirectoryBlockInfo* directory_block_info = directory_entry->getDirectoryBlockInfo();
   DirectoryState::dstate_t curr_dstate = directory_block_info->getDState();

   switch (curr_dstate)
   {
      case DirectoryState::MODIFIED:
         {
            assert(cached_data_buf == NULL);
            getMemoryManager()->sendMsg(ShmemMsg::WB_REQ, 
                  MemComponent::DRAM_DIR, MemComponent::L2_CACHE, 
                  requester /* requester */, 
                  directory_entry->getOwner() /* receiver */, 
                  address);
         }
         break;
   
      case DirectoryState::SHARED:
         {
            bool add_result = directory_entry->addSharer(requester);
            if (add_result == false)
            {
               assert(!cached_data_buf);
               core_id_t sharer_id = directory_entry->getOneSharer();
               // Send a message to another sharer to invalidate that
               getMemoryManager()->sendMsg(ShmemMsg::INV_REQ, 
                     MemComponent::DRAM_DIR, MemComponent::L2_CACHE, 
                     requester /* requester */, 
                     sharer_id /* receiver */, 
                     address);
            }
            else
            {
               if (cached_data_buf)
               {
                  // Send Data to L2 Cache
                  retrieveDataAndSendToL2Cache(ShmemMsg::SH_REP, requester, address, cached_data_buf);
     
                  // Process Next Request
                  scheduleNextReqFromL2Cache(address);
               }
               else
               {
                  // Remove myself temporarily till data is fetched
                  directory_entry->removeSharer(requester);
                  assert(directory_entry->getNumSharers() > 0);

                  // Get Data From Dram
                  getDataFromDram(address, requester);
               }
            }
         }
         break;

      case DirectoryState::UNCACHED:
         {
            if (cached_data_buf)
            {
               // Modifiy the directory entry contents
               bool add_result = directory_entry->addSharer(requester);
               assert(add_result == true);
               directory_block_info->setDState(DirectoryState::SHARED);

               // Send Data to L2 Cache
               retrieveDataAndSendToL2Cache(ShmemMsg::SH_REP, requester, address, cached_data_buf);
     
               // Process Next Request
               scheduleNextReqFromL2Cache(address);
            }

            else
            {
               // Get Data From Dram
               getDataFromDram(address, requester);
            }
         }
         break;
   
      default:
         LOG_PRINT_ERROR("Unsupported Directory State: %u", curr_dstate);
         break;
   }
}

void
DramDirectoryCntlr::retrieveDataAndSendToL2Cache(ShmemMsg::msg_t reply_msg_type,
      core_id_t receiver, IntPtr address, Byte* cached_data_buf)
{
   assert(cached_data_buf);
      
   // I already have the data I need cached
   getMemoryManager()->sendMsg(reply_msg_type, 
         MemComponent::DRAM_DIR, MemComponent::L2_CACHE, 
         receiver /* requester */, 
         receiver /* receiver */, 
         address,
         cached_data_buf, getCacheBlockSize());
}

void
DramDirectoryCntlr::handleMsgFromDram(core_id_t sender, ShmemMsg* shmem_msg)
{
   // Schedule the request according to the Dram Directory Contention Delays
   scheduleRequest(getCoreId(), shmem_msg);
}

void
DramDirectoryCntlr::__handleMsgFromDram(core_id_t sender, ShmemMsg* shmem_msg)
{
   // Account for Dram Directory Cache Access Delay
   assert (getShmemPerfModel());
   getShmemPerfModel()->incrCycleCount(getDramDirectoryCache()->getAccessDelay());

   IntPtr address = shmem_msg->getAddress();
   assert(shmem_msg->getMsgType() == ShmemMsg::GET_DATA_REP);

   // Outstanding Dram Req
   assert(m_dram_req_outstanding_set.find(address) != m_dram_req_outstanding_set.end());
   m_dram_req_outstanding_set.erase(address);

   DirectoryEntry* directory_entry = m_dram_directory_cache->getDirectoryEntry(address);
   assert(directory_entry);

   DirectoryBlockInfo* directory_block_info = directory_entry->getDirectoryBlockInfo();
   
   assert(m_dram_directory_req_queue_list->size(address) > 0);
   ShmemReq* shmem_req = m_dram_directory_req_queue_list->front(address);
   // Update Time
   shmem_req->setTime(getShmemPerfModel()->getCycleCount());

   if (shmem_req->getShmemMsg()->getMsgType() == ShmemMsg::EX_REQ)
   {
      LOG_ASSERT_ERROR(directory_block_info->getDState() == DirectoryState::UNCACHED,
            "Directory State(%u)", directory_block_info->getDState());
      processExReqFromL2Cache(shmem_req, shmem_msg->getDataBuf());
   }
   else if (shmem_req->getShmemMsg()->getMsgType() == ShmemMsg::SH_REQ)
   {
      LOG_ASSERT_ERROR(directory_block_info->getDState() != DirectoryState::MODIFIED,
            "Directory State(%u)", directory_block_info->getDState());
      processShReqFromL2Cache(shmem_req, shmem_msg->getDataBuf());
   }
   else
   {
      LOG_PRINT_ERROR("Unrecognized Shmem Req Type(%u)", shmem_req->getShmemMsg()->getMsgType());
   }
}

void
DramDirectoryCntlr::processInvRepFromL2Cache(core_id_t sender, ShmemMsg* shmem_msg)
{
   IntPtr address = shmem_msg->getAddress();

   DirectoryEntry* directory_entry = m_dram_directory_cache->getDirectoryEntry(address);
   assert(directory_entry);

   DirectoryBlockInfo* directory_block_info = directory_entry->getDirectoryBlockInfo();
   LOG_ASSERT_ERROR(directory_block_info->getDState() == DirectoryState::SHARED,
         "Directory State(%u)", directory_block_info->getDState());

   directory_entry->removeSharer(sender);
   if (directory_entry->getNumSharers() == 0)
   {
      directory_block_info->setDState(DirectoryState::UNCACHED);
   }

   if ( (m_dram_directory_req_queue_list->size(address) > 0) && (isActive(address)) )
   {
      ShmemReq* shmem_req = m_dram_directory_req_queue_list->front(address);

      // Update Times in the Shmem Perf Model and the Shmem Req
      shmem_req->setTime(getShmemPerfModel()->getCycleCount());

      if (shmem_req->getShmemMsg()->getMsgType() == ShmemMsg::EX_REQ)
      {
         // An ShmemMsg::EX_REQ caused the invalidation
         if (directory_block_info->getDState() == DirectoryState::UNCACHED)
         {
            processExReqFromL2Cache(shmem_req);
         }
      }
      else if (shmem_req->getShmemMsg()->getMsgType() == ShmemMsg::SH_REQ)
      {
         if (m_dram_req_outstanding_set.find(address) == m_dram_req_outstanding_set.end())
         {
            // A ShmemMsg::SH_REQ caused the invalidation
            processShReqFromL2Cache(shmem_req);
         }
      }
      else // shmem_req->getShmemMsg()->getMsgType() == ShmemMsg::NULLIFY_REQ
      {
         if (directory_block_info->getDState() == DirectoryState::UNCACHED)
         {
            processNullifyReq(shmem_req);
         }
      }
   }
}

void
DramDirectoryCntlr::processFlushRepFromL2Cache(core_id_t sender, ShmemMsg* shmem_msg)
{
   IntPtr address = shmem_msg->getAddress();

   DirectoryEntry* directory_entry = m_dram_directory_cache->getDirectoryEntry(address);
   assert(directory_entry);

   DirectoryBlockInfo* directory_block_info = directory_entry->getDirectoryBlockInfo();
   assert(directory_block_info->getDState() == DirectoryState::MODIFIED);

   directory_entry->removeSharer(sender);
   directory_entry->setOwner(INVALID_CORE_ID);
   directory_block_info->setDState(DirectoryState::UNCACHED);

   if ( (m_dram_directory_req_queue_list->size(address) != 0) && (isActive(address)) )
   {
      ShmemReq* shmem_req = m_dram_directory_req_queue_list->front(address);

      // Update times
      shmem_req->setTime(getShmemPerfModel()->getCycleCount());

      // An involuntary/voluntary Flush
      if (shmem_req->getShmemMsg()->getMsgType() == ShmemMsg::EX_REQ)
      {
         processExReqFromL2Cache(shmem_req, shmem_msg->getDataBuf());
      }
      else if (shmem_req->getShmemMsg()->getMsgType() == ShmemMsg::SH_REQ)
      {
         // Write Data to Dram
         putDataToDram(address, shmem_msg->getRequester(), shmem_msg->getDataBuf());
         processShReqFromL2Cache(shmem_req, shmem_msg->getDataBuf());
      }
      else // shmem_req->getShmemMsg()->getMsgType() == ShmemMsg::NULLIFY_REQ
      {
         // Write Data To Dram
         putDataToDram(address, shmem_msg->getRequester(), shmem_msg->getDataBuf());
         processNullifyReq(shmem_req);
      }
   }
   else
   {
      // This was just an eviction
      // Write Data to Dram
      putDataToDram(address, shmem_msg->getRequester(), shmem_msg->getDataBuf());
   }
}

void
DramDirectoryCntlr::processWbRepFromL2Cache(core_id_t sender, ShmemMsg* shmem_msg)
{
   IntPtr address = shmem_msg->getAddress();

   DirectoryEntry* directory_entry = m_dram_directory_cache->getDirectoryEntry(address);
   assert(directory_entry);
   
   DirectoryBlockInfo* directory_block_info = directory_entry->getDirectoryBlockInfo();

   assert(directory_block_info->getDState() == DirectoryState::MODIFIED);
   assert(directory_entry->hasSharer(sender));
   
   directory_entry->setOwner(INVALID_CORE_ID);
   directory_block_info->setDState(DirectoryState::SHARED);

   if ( (m_dram_directory_req_queue_list->size(address) != 0) && (isActive(address)) )
   {
      ShmemReq* shmem_req = m_dram_directory_req_queue_list->front(address);
      LOG_ASSERT_ERROR(shmem_req->getShmemMsg()->getMsgType() == ShmemMsg::SH_REQ,
            "Address(0x%x), Req(%u)",
            address, shmem_req->getShmemMsg()->getMsgType());

      // Update Time
      shmem_req->setTime(getShmemPerfModel()->getCycleCount());

      // Write Data to Dram
      putDataToDram(address, shmem_msg->getRequester(), shmem_msg->getDataBuf());

      processShReqFromL2Cache(shmem_req, shmem_msg->getDataBuf());
   }
   else
   {
      LOG_PRINT_ERROR("Should not reach here");
   }
}

void
DramDirectoryCntlr::getDataFromDram(IntPtr address, core_id_t requester)
{
   // Insert that the request is outstanding
   assert(m_dram_req_outstanding_set.find(address) == m_dram_req_outstanding_set.end());
   m_dram_req_outstanding_set.insert(address);

   // Get Data From Dram
   getMemoryManager()->sendMsg(ShmemMsg::GET_DATA_REQ,
         MemComponent::DRAM_DIR, MemComponent::DRAM,
         requester /* requester */,
         getMemoryManager()->getCore()->getId() /* receiver */,
         address);
}

void
DramDirectoryCntlr::putDataToDram(IntPtr address, core_id_t requester, Byte* data_buf)
{
   // Put Data To Dram
   getMemoryManager()->sendMsg(ShmemMsg::PUT_DATA_REQ,
         MemComponent::DRAM_DIR, MemComponent::DRAM,
         requester /* requester */,
         getMemoryManager()->getCore()->getId() /* receiver */,
         address,
         data_buf, getCacheBlockSize());
}

bool
DramDirectoryCntlr::isActive(IntPtr address)
{
   return (m_inactive_address_set.find(address) == m_inactive_address_set.end());
}

core_id_t
DramDirectoryCntlr::getCoreId()
{
   return getMemoryManager()->getCore()->getId();
}

UInt32
DramDirectoryCntlr::getCacheBlockSize()
{
   return getMemoryManager()->getCacheBlockSize();
}

ShmemPerfModel*
DramDirectoryCntlr::getShmemPerfModel()
{
   return getMemoryManager()->getShmemPerfModel();
}

}
