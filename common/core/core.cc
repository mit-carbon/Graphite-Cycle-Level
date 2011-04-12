#include "core.h"
#include "network.h"
#include "syscall_model.h"
#include "sync_client.h"
#include "network_types.h"
#include "memory_manager.h"
#include "pin_memory_manager.h"
#include "clock_skew_minimization_object.h"
#include "performance_model.h"
#include "simulator.h"
#include "event.h"
#include "log.h"

using namespace std;

Lock Core::m_global_core_lock;

Core::Core(SInt32 id)
   : m_core_id(id)
   , m_core_state(IDLE)
   , m_last_memory_access_id(0)
{
   LOG_PRINT("Core ctor for: %d", id);

   m_network = new Network(this);

   m_performance_model = PerformanceModel::create(this);

   if (Config::getSingleton()->isSimulatingSharedMemory())
   {
      LOG_PRINT("instantiated shared memory performance model");
      m_shmem_perf_model = new ShmemPerfModel();

      LOG_PRINT("instantiated memory manager model");
      m_memory_manager = MemoryManager::createMMU(
            Sim()->getCfg()->getString("caching_protocol/type"),
            this, m_network, m_shmem_perf_model);

      m_pin_memory_manager = new PinMemoryManager(this);
   }
   else
   {
      m_shmem_perf_model = (ShmemPerfModel*) NULL;
      m_memory_manager = (MemoryManager *) NULL;
      m_pin_memory_manager = (PinMemoryManager*) NULL;

      LOG_PRINT("No Memory Manager being used");
   }

   m_syscall_model = new SyscallMdl(m_network);
   m_sync_client = new SyncClient(this);

   m_clock_skew_minimization_client = ClockSkewMinimizationClient::create(Sim()->getCfg()->getString("clock_skew_minimization/scheme","none"), this);
}

Core::~Core()
{
   if (m_clock_skew_minimization_client)
      delete m_clock_skew_minimization_client;

   delete m_sync_client;
   delete m_syscall_model;
   if (Config::getSingleton()->isSimulatingSharedMemory())
   {
      delete m_pin_memory_manager;
      delete m_memory_manager;
      delete m_shmem_perf_model;
   }
   delete m_performance_model;
   LOG_PRINT("Deleted performance mode");
   delete m_network;
   LOG_PRINT("Deleted network");
}

void Core::outputSummary(std::ostream &os)
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
   LOG_PRINT("outputSummary() end");
}

int Core::coreSendW(int sender, int receiver, char* buffer, int size, carbon_network_t net_type)
{
   PacketType pkt_type = getPktTypeFromUserNetType(net_type);

   SInt32 sent;
   if (receiver == CAPI_ENDPOINT_ALL)
      sent = m_network->netBroadcast(pkt_type, buffer, size);
   else
      sent = m_network->netSend(receiver, pkt_type, buffer, size);
   
   LOG_ASSERT_ERROR(sent == size, "Bytes Sent(%i), Message Size(%i)", sent, size);

   return sent == size ? 0 : -1;
}

int Core::coreRecvW(int sender, int receiver, char* buffer, int size, carbon_network_t net_type)
{
   PacketType pkt_type = getPktTypeFromUserNetType(net_type);

   NetPacket* packet;
   if (sender == CAPI_ENDPOINT_ANY)
      packet = m_network->netRecvType(pkt_type);
   else
      packet = m_network->netRecv(sender, pkt_type);

   LOG_PRINT("Got packet: from %i, to %i, type %i, len %i",
         packet->sender, packet->receiver, (SInt32)packet->type, packet->length);

   LOG_ASSERT_ERROR((unsigned)size == packet->length,
         "Core: User thread requested packet of size: %d, got a packet from %d of size: %d",
         size, sender, packet->length);

   memcpy(buffer, packet->data, size);

   // De-allocate dynamic memory
   // Is this the best place to de-allocate packet.data ??
   packet->release();

   return 0;
}

PacketType Core::getPktTypeFromUserNetType(carbon_network_t net_type)
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
   if (m_clock_skew_minimization_client)
      m_clock_skew_minimization_client->enable();

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
   if (m_clock_skew_minimization_client)
      m_clock_skew_minimization_client->disable();

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

// Models must be disabled when calling this function
void Core::resetPerformanceModels()
{
   if (m_clock_skew_minimization_client)
      m_clock_skew_minimization_client->reset();

   if (Config::getSingleton()->isSimulatingSharedMemory())
   {
      getShmemPerfModel()->reset();
      getMemoryManager()->resetModels();
   }
   getNetwork()->resetModels();
   if (Config::getSingleton()->getEnablePerformanceModeling())
   {
      getPerformanceModel()->reset();
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
                           MemComponent::component_t mem_component,
                           lock_signal_t lock_signal,
                           mem_op_t mem_op_type,
                           IntPtr address,
                           Byte* data_buffer,
                           UInt32 bytes,
                           bool modeled)
{
   assert(bytes >= 0);
  
   UInt64 curr_time;
   if (Config::getSingleton()->getSimulationMode() == Config::CYCLE_ACCURATE)
      curr_time = time;
   else
      curr_time = (time == 0) ? getPerformanceModel()->getCycleCount() : time;
    
   MemoryAccessStatus* memory_access_status = 
      new MemoryAccessStatus(m_last_memory_access_id ++, curr_time, address, bytes,
                             mem_component, lock_signal, mem_op_type,
                             data_buffer, modeled);
   m_memory_access_status_map.insert(make_pair<UInt32, MemoryAccessStatus*>
                                    (memory_access_status->_access_id, memory_access_status));

   continueMemoryAccess(*memory_access_status);
}

void
Core::completeCacheAccess(UInt64 time, SInt32 memory_access_id)
{
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
   UnstructuredBuffer event_args;
   event_args << getMemoryManager()
              << memory_access_status._access_id
              << memory_access_status._mem_component
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
   if (memory_access_status._modeled)
   {
      UInt64 memory_latency = memory_access_status._curr_time - memory_access_status._start_time;
      
      getShmemPerfModel()->incrTotalMemoryAccessLatency(memory_latency);
      
      DynamicInstructionInfo info = DynamicInstructionInfo::createMemoryInfo(memory_latency,
                                    memory_access_status._start_address,
                                    (memory_access_status._mem_op_type == WRITE) ? Operand::WRITE : Operand::READ);
       
      UnstructuredBuffer event_args;
      event_args << this << info; 
      EventCompleteMemoryAccess* event = new EventCompleteMemoryAccess(memory_access_status._curr_time,
                                                                       event_args);
      Event::processInOrder(event, m_core_id, EventQueue::ORDERED);
   }

   // Remove memory_access_status from the map
   m_memory_access_status_map.erase(memory_access_status._access_id);
   // De-allocate the structure
   delete &memory_access_status;
}

// FIXME: This should actually be 'accessDataMemory()'
/*
 * accessMemory (lock_signal_t lock_signal, mem_op_t mem_op_type, IntPtr d_addr, char* data_buffer, UInt32 data_size, bool modeled)
 *
 * Arguments:
 *   lock_signal :: NONE, LOCK, or UNLOCK
 *   mem_op_type :: READ, READ_EX, or WRITE
 *   d_addr :: address of location we want to access (read or write)
 *   data_buffer :: buffer holding data for WRITE or buffer which must be written on a READ
 *   data_size :: size of data we must read/write
 *   modeled :: says whether it is modeled or not
 *
 * Return Value:
 *   number of misses :: State the number of cache misses
 */
void
Core::accessMemory(lock_signal_t lock_signal, mem_op_t mem_op_type,
      IntPtr d_addr, char* data_buffer, UInt32 data_size, bool modeled)
{
   if (Config::getSingleton()->isSimulatingSharedMemory())
   {
      return initiateMemoryAccess(0, MemComponent::L1_DCACHE, lock_signal, mem_op_type,
            d_addr, (Byte*) data_buffer, data_size, modeled);
   }
   
   else
   {   
      return nativeMemOp(lock_signal, mem_op_type, d_addr, data_buffer, data_size);
   }
}


void
Core::nativeMemOp(lock_signal_t lock_signal, mem_op_t mem_op_type,
      IntPtr d_addr, char* data_buffer, UInt32 data_size)
{
   if (data_size <= 0)
   {
      return;
   }

   if (lock_signal == LOCK)
   {
      assert(mem_op_type == READ_EX);
      m_global_core_lock.acquire();
   }

   if ( (mem_op_type == READ) || (mem_op_type == READ_EX) )
   {
      memcpy ((void*) data_buffer, (void*) d_addr, (size_t) data_size);
   }
   else if (mem_op_type == WRITE)
   {
      memcpy ((void*) d_addr, (void*) data_buffer, (size_t) data_size);
   }

   if (lock_signal == UNLOCK)
   {
      assert(mem_op_type == WRITE);
      m_global_core_lock.release();
   }
}

Core::State 
Core::getState()
{
   ScopedLock scoped_lock(m_core_state_lock);
   return m_core_state;
}

void
Core::setState(State core_state)
{
   ScopedLock scoped_lock(m_core_state_lock);
   m_core_state = core_state;
}
