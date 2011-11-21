#include "core.h"
#include "network.h"
#include "network_types.h"
#include "memory_manager.h"
#include "performance_model.h"
#include "simulator.h"
#include "event.h"
#include "syscall_client.h"
#include "log.h"

using namespace std;

Core::Core(core_id_t id)
   : m_core_id(id)
{
   LOG_PRINT("Core ctor for: %d", id);

   m_network = new Network(this);

   m_performance_model = PerformanceModel::create(this);

   if (Config::getSingleton()->isSimulatingSharedMemory())
   {
      m_shmem_perf_model = new ShmemPerfModel();
      LOG_PRINT("instantiated shared memory performance model");

      m_memory_manager = MemoryManager::createMMU(
            Sim()->getCfg()->getString("caching_protocol/type"),
            this, m_network, m_shmem_perf_model);
      LOG_PRINT("instantiated memory manager model");
   }
   else
   {
      m_shmem_perf_model = (ShmemPerfModel*) NULL;
      m_memory_manager = (MemoryManager *) NULL;
      LOG_PRINT("No Memory Manager being used");
   }

   m_syscall_client = new SyscallClient();

   // Output Summary Callback
   m_external_output_summary_callback = (OutputSummaryCallback) NULL;
   m_external_callback_obj = (void*) NULL;
}

Core::~Core()
{
   LOG_PRINT("Core dtor start");

   delete m_syscall_client;
   LOG_PRINT("Deleted Syscall Client");
   if (Config::getSingleton()->isSimulatingSharedMemory())
   {
      LOG_PRINT("Deleted Pin Memory Manager");
      delete m_memory_manager;
      LOG_PRINT("Deleted Memory Manager");
      delete m_shmem_perf_model;
      LOG_PRINT("Deleted Shmem Perf Model");
   }
   delete m_performance_model;
   LOG_PRINT("Deleted performance mode");
   delete m_network;
   LOG_PRINT("Deleted network");

   LOG_PRINT("Core dtor end");
}

void
Core::outputSummary(std::ostream &os)
{
   LOG_PRINT("outputSummary() start");
   if (Config::getSingleton()->getEnablePerformanceModeling())
   {
      LOG_PRINT("PerformanceModel::outputSummary() printed");
      getPerformanceModel()->outputSummary(os);
   }
   getNetwork()->outputSummary(os);

   if (Config::getSingleton()->isSimulatingSharedMemory())
   {
      getShmemPerfModel()->outputSummary(os, Config::getSingleton()->getCoreFrequency(getId()));
      getMemoryManager()->outputSummary(os);
   }

   // Call external callback function for output summary
   if (m_external_output_summary_callback)
      m_external_output_summary_callback(m_external_callback_obj, os);
   
   LOG_PRINT("outputSummary() end");
}

void
Core::registerExternalOutputSummaryCallback(OutputSummaryCallback callback, void* callback_obj)
{
   m_external_output_summary_callback = callback;
   m_external_callback_obj = callback_obj;
}

void
Core::unregisterExternalOutputSummaryCallback(OutputSummaryCallback callback)
{
   assert(m_external_output_summary_callback == callback);
   m_external_output_summary_callback = (OutputSummaryCallback) NULL;
   m_external_callback_obj = (void*) NULL;
}

void
Core::sendMsg(core_id_t sender, core_id_t receiver, char* buffer, SInt32 size, carbon_network_t net_type)
{
   CAPI_return_t ret_val = 0;

   if ((sender != INVALID_CORE_ID) && (receiver != INVALID_CORE_ID))
   {
      PacketType pkt_type = getPktTypeFromUserNetType(net_type);

      SInt32 sent;
      if (receiver == CAPI_ENDPOINT_ALL)
         sent = m_network->netBroadcast(pkt_type, buffer, size);
      else
         sent = m_network->netSend(receiver, pkt_type, buffer, size);
      
      LOG_ASSERT_ERROR(sent == size, "Bytes Sent(%i), Message Size(%i)", sent, size);

      // Send Reply to Sim Thread
      ret_val = (sent == size) ? 0 : -1;
   }
   else
   {
      if (sender == INVALID_CORE_ID)
         ret_val = CAPI_SenderNotInitialized;
      else if (receiver == INVALID_CORE_ID)
         ret_val = CAPI_ReceiverNotInitialized;
   }

   Sim()->getThreadInterface(m_core_id)->sendSimReply(getPerformanceModel()->getTime(), (IntPtr) ret_val);
}

void
Core::recvMsg(core_id_t sender, core_id_t receiver, char* buffer, SInt32 size, carbon_network_t net_type)
{
   if ((sender != INVALID_CORE_ID) && (receiver != INVALID_CORE_ID))
   {
      PacketType pkt_type = getPktTypeFromUserNetType(net_type);

      m_recv_buffer = RecvBuffer(buffer, size);
   
      NetMatch net_match;
      net_match = (sender == CAPI_ENDPOINT_ANY) ? NetMatch(pkt_type) : NetMatch(sender, pkt_type);
      m_network->netRecv(net_match, coreRecvMsg, this);
   }
   else
   {
      CAPI_return_t ret_val = 0;
      if (sender == INVALID_CORE_ID)
         ret_val = CAPI_SenderNotInitialized;
      else if (receiver == INVALID_CORE_ID)
         ret_val = CAPI_ReceiverNotInitialized;

      Sim()->getThreadInterface(m_core_id)->sendSimReply(getPerformanceModel()->getTime(), (IntPtr) ret_val);
   }
}

void
coreRecvMsg(void* obj, NetPacket packet)
{
   Core* core = (Core*) obj;

   // Copy the Received Data
   core->__recvMsg(packet);
}

void
Core::__recvMsg(const NetPacket& packet)
{  
   char* buffer = m_recv_buffer._buffer;
   SInt32 size = m_recv_buffer._size;  

   LOG_PRINT("Got packet: from %i, to %i, type %i, len %i",
         packet.sender, packet.receiver, (SInt32)packet.type, packet.length);

   LOG_ASSERT_ERROR(size == (SInt32) packet.length,
         "Core: User thread requested packet of size: %i, got a packet from %i of size: %i",
         size, packet.sender, (SInt32) packet.length);

   // Copy the data to internal buffers
   memcpy(buffer, packet.data, size);

   // Send Reply to Sim Thread
   Sim()->getThreadInterface(m_core_id)->sendSimReply(packet.time, 0);
}

PacketType
Core::getPktTypeFromUserNetType(carbon_network_t net_type)
{
   switch(net_type)
   {
      case CARBON_NET_USER_1:
         return USER_1;

      case CARBON_NET_USER_2:
         return USER_2;

      default:
         LOG_PRINT_ERROR("Unrecognized User Network(%u)", net_type);
         return (PacketType) -1;
   }
}

void Core::enablePerformanceModels()
{
   if (Config::getSingleton()->isSimulatingSharedMemory())
   {
      getShmemPerfModel()->enable();
      getMemoryManager()->enableModels();
   }
   getNetwork()->enableModels();
   if (Config::getSingleton()->getEnablePerformanceModeling())
   {
      getPerformanceModel()->enable();
   }
}

void Core::disablePerformanceModels()
{
   if (Config::getSingleton()->isSimulatingSharedMemory())
   {
      getShmemPerfModel()->disable();
      getMemoryManager()->disableModels();
   }
   getNetwork()->disableModels();
   if (Config::getSingleton()->getEnablePerformanceModeling())
   {
      getPerformanceModel()->disable();
   }
}

void
Core::updateInternalVariablesOnFrequencyChange(volatile float frequency)
{
   getPerformanceModel()->updateInternalVariablesOnFrequencyChange(frequency);
   if (Config::getSingleton()->isSimulatingSharedMemory())
   {
      getShmemPerfModel()->updateInternalVariablesOnFrequencyChange(frequency);
      getMemoryManager()->updateInternalVariablesOnFrequencyChange(frequency);
   }
}

void
Core::initiateMemoryAccess(UInt64 time,
                           UInt32 memory_access_id,
                           MemComponent::component_t mem_component,
                           lock_signal_t lock_signal,
                           mem_op_t mem_op_type,
                           IntPtr address,
                           Byte* data_buffer,
                           UInt32 bytes,
                           bool modeled)
{
   LOG_PRINT("Initiate Memory Access [Core Id(%i), Time(%llu), Mem Component(%u), Lock Signal(%u), Mem Op Type(%u), Address(0x%llx), Data Buffer(%p), Bytes(%u), Modeled(%s)]",
         m_core_id, time, mem_component, lock_signal, mem_op_type, address, data_buffer, bytes, modeled ? "TRUE" : "FALSE");

   assert(bytes >= 0);
  
   UInt64 curr_time = time;
    
   MemoryAccessStatus* memory_access_status = 
      new MemoryAccessStatus(memory_access_id, curr_time, address, bytes,
                             mem_component, lock_signal, mem_op_type,
                             data_buffer, modeled);
   m_memory_access_status_map.insert(make_pair<UInt32, MemoryAccessStatus*>
                                    (memory_access_status->_access_id, memory_access_status));

   continueMemoryAccess(*memory_access_status);
}

void
Core::completeCacheAccess(UInt64 time, UInt32 memory_access_id)
{
   LOG_PRINT("Complete Cache Access [Core Id(%i), Time(%llu), Access Id(%u)]", m_core_id, time, memory_access_id);

   MemoryAccessStatus& memory_access_status = *m_memory_access_status_map[memory_access_id];
   memory_access_status._curr_address += memory_access_status._curr_bytes;
   memory_access_status._bytes_remaining -= memory_access_status._curr_bytes;
   memory_access_status._data_buffer += memory_access_status._curr_bytes;
   memory_access_status._curr_time = time;

   continueMemoryAccess(memory_access_status);
}

void
Core::continueMemoryAccess(MemoryAccessStatus& memory_access_status)
{
   LOG_PRINT("Continue Memory Access [Core Id(%i), Access Id(%u), Bytes Remaining(%u)]",
         m_core_id, memory_access_status._access_id, memory_access_status._bytes_remaining);

   if (memory_access_status._bytes_remaining == 0)
   {
      completeMemoryAccess(memory_access_status);
      return;
   }

   UInt32 cache_block_size = getMemoryManager()->getCacheBlockSize();
   IntPtr address_aligned = (memory_access_status._curr_address / cache_block_size) * cache_block_size;
   UInt32 offset = memory_access_status._curr_address - address_aligned;
   memory_access_status._curr_bytes = (memory_access_status._bytes_remaining > (cache_block_size - offset)) ?
                                      (cache_block_size - offset) : memory_access_status._bytes_remaining;
   
   // If it is a READ or READ_EX operation,
   // 'accessL1Cache' causes data_buffer
   // to be automatically filled in
   // If it is a WRITE operation,
   // 'accessL1Cache' reads the data
   // from data_buffer
   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) << getMemoryManager()
                 << memory_access_status._mem_component
                 << memory_access_status._access_id
                 << memory_access_status._lock_signal << memory_access_status._mem_op_type
                 << address_aligned << offset
                 << memory_access_status._data_buffer << memory_access_status._curr_bytes
                 << memory_access_status._modeled;

   EventInitiateCacheAccess* event = new EventInitiateCacheAccess(memory_access_status._curr_time, event_args);
   Event::processInOrder(event, m_core_id, EventQueue::ORDERED);
}

void
Core::completeMemoryAccess(MemoryAccessStatus& memory_access_status)
{
   LOG_PRINT("Complete Memory Access [Core(%i), Access Id(%u), Start Address(0x%llx), Total Bytes(%u), Modeled(%s)]",
         m_core_id, memory_access_status._access_id, memory_access_status._start_address, memory_access_status._total_bytes,
         (memory_access_status._modeled) ? "TRUE" : "FALSE");

   if (memory_access_status._modeled)
   {
      UInt64 memory_latency = memory_access_status._curr_time - memory_access_status._start_time;
      
      getShmemPerfModel()->incrTotalMemoryAccessLatency(memory_latency);
      
      UnstructuredBuffer* event_args = new UnstructuredBuffer();
      (*event_args) << this << memory_access_status._access_id; 
      EventCompleteMemoryAccess* event = new EventCompleteMemoryAccess(memory_access_status._curr_time,
                                                                       event_args);
      Event::processInOrder(event, m_core_id, EventQueue::ORDERED);
   }

   // Remove memory_access_status from the map
   m_memory_access_status_map.erase(memory_access_status._access_id);
   // De-allocate the structure
   delete &memory_access_status;
}
