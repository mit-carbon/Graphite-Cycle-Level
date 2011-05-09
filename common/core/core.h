#pragma once

#include <string.h>

// some forward declarations for cross includes
class Network;
class MemoryManager;
class SyscallClient;
class PerformanceModel;
// FIXME: Move this out of here eventually
class PinMemoryManager;

#include "mem_component.h"
#include "fixed_types.h"
#include "config.h"
#include "performance_model.h"
#include "shmem_perf_model.h"
#include "capi.h"
#include "packet_type.h"
#include "network.h"

using namespace std;

class Core
{
public:
   enum Status
   {
      RUNNING = 0,
      IDLE,
      NUM_STATES
   };

   enum lock_signal_t
   {
      INVALID_LOCK_SIGNAL = 0,
      MIN_LOCK_SIGNAL,
      NONE = MIN_LOCK_SIGNAL,
      LOCK,
      UNLOCK,
      MAX_LOCK_SIGNAL = UNLOCK,
      NUM_LOCK_SIGNAL_TYPES = MAX_LOCK_SIGNAL - MIN_LOCK_SIGNAL + 1
   };
  
   enum mem_op_t
   {
      INVALID_MEM_OP = 0,
      MIN_MEM_OP,
      READ = MIN_MEM_OP,
      READ_EX,
      WRITE,
      MAX_MEM_OP = WRITE,
      NUM_MEM_OP_TYPES = MAX_MEM_OP - MIN_MEM_OP + 1
   };

   Core(core_id_t id);
   ~Core();

   void outputSummary(std::ostream &os);

   // User Messages
   void sendMsg(core_id_t sender, core_id_t receiver, char *buffer, SInt32 size, carbon_network_t net_type);
   void recvMsg(core_id_t sender, core_id_t receiver, char *buffer, SInt32 size, carbon_network_t net_type);
   void __recvMsg(const NetPacket& packet);

  
   void initiateMemoryAccess(UInt64 time, UInt32 memory_access_id, MemComponent::component_t mem_component,
         lock_signal_t lock_signal, mem_op_t mem_op_type,
         IntPtr address, Byte* data_buffer, UInt32 bytes, bool modeled = false);
   void completeCacheAccess(UInt64 time, UInt32 memory_access_id);

   // network accessor since network is private
   int getId() { return m_core_id; }
   Network *getNetwork() { return m_network; }
   PerformanceModel *getPerformanceModel() { return m_performance_model; }
   MemoryManager *getMemoryManager() { return m_memory_manager; }
   PinMemoryManager *getPinMemoryManager() { return m_pin_memory_manager; }
   SyscallClient *getSyscallClient() { return m_syscall_client; }
   ShmemPerfModel* getShmemPerfModel() { return m_shmem_perf_model; }

   void updateInternalVariablesOnFrequencyChange(float frequency);

   void enablePerformanceModels();
   void disablePerformanceModels();

private:
   
   class MemoryAccessStatus
   {
   public:
      MemoryAccessStatus(UInt32 access_id, UInt64 time,
                         IntPtr address, UInt32 bytes,
                         MemComponent::component_t mem_component,
                         lock_signal_t lock_signal,
                         mem_op_t mem_op_type,
                         Byte* data_buffer, bool modeled)
         : _access_id(access_id)
         , _start_time(time)
         , _curr_time(time)
         , _start_address(address)
         , _curr_address(address)
         , _total_bytes(bytes)
         , _curr_bytes(0)
         , _bytes_remaining(bytes)
         , _mem_component(mem_component)
         , _lock_signal(lock_signal)
         , _mem_op_type(mem_op_type)
         , _data_buffer(data_buffer)
         , _modeled(modeled)
      {}

      ~MemoryAccessStatus() {}

      UInt32 _access_id;
      UInt64 _start_time;
      UInt64 _curr_time;
      IntPtr _start_address;
      IntPtr _curr_address;
      UInt32 _total_bytes;
      UInt32 _curr_bytes;
      UInt32 _bytes_remaining;
      MemComponent::component_t _mem_component;
      lock_signal_t _lock_signal;
      mem_op_t _mem_op_type;
      Byte* _data_buffer;
      bool _modeled;
   };

   class RecvBuffer
   {
   public:
      RecvBuffer() : _buffer(NULL), _size(0) {}
      RecvBuffer(char* buffer, SInt32 size) : _buffer(buffer), _size(size) {}
      char* _buffer;
      SInt32 _size;
   };

   core_id_t m_core_id;
   MemoryManager *m_memory_manager;
   PinMemoryManager *m_pin_memory_manager;
   Network *m_network;
   PerformanceModel *m_performance_model;
   SyscallClient *m_syscall_client;
   ShmemPerfModel* m_shmem_perf_model;
  
   // Memory Access Status
   std::map<UInt32, MemoryAccessStatus*> m_memory_access_status_map;

   // Recv Buffer
   RecvBuffer m_recv_buffer;
   
   PacketType getPktTypeFromUserNetType(carbon_network_t net_type);

   // Memory Access
   void continueMemoryAccess(MemoryAccessStatus& memory_access_status);
   void completeMemoryAccess(MemoryAccessStatus& memory_access_status);
};

void coreRecvMsg(void* obj, NetPacket packet);
