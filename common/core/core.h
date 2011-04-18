#ifndef CORE_H
#define CORE_H

#include <string.h>

// some forward declarations for cross includes
class Network;
class MemoryManager;
class SyscallMdl;
class SyncClient;
class ClockSkewMinimizationClient;
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

using namespace std;

class Core
{
public:
   enum State
   {
      RUNNING = 0,
      INITIALIZING,
      STALLED,
      SLEEPING,
      WAKING_UP,
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

   Core(SInt32 id);
   ~Core();

   void outputSummary(std::ostream &os);

   int coreSendW(int sender, int receiver, char *buffer, int size, carbon_network_t net_type);
   int coreRecvW(int sender, int receiver, char *buffer, int size, carbon_network_t net_type);
  
   void initiateMemoryAccess(UInt64 time,
         MemComponent::component_t mem_component,
         lock_signal_t lock_signal,
         mem_op_t mem_op_type,
         IntPtr address,
         Byte* data_buffer,
         UInt32 bytes,
         bool modeled = false);
   void completeCacheAccess(UInt64 time, UInt32 memory_access_id);

   void accessMemory(lock_signal_t lock_signal, mem_op_t mem_op_type,
         IntPtr d_addr, char* data_buffer, UInt32 data_size, bool modeled = false);
   void nativeMemOp(lock_signal_t lock_signal, mem_op_t mem_op_type,
         IntPtr d_addr, char* data_buffer, UInt32 data_size);

   // network accessor since network is private
   int getId() { return m_core_id; }
   Network *getNetwork() { return m_network; }
   PerformanceModel *getPerformanceModel() { return m_performance_model; }
   MemoryManager *getMemoryManager() { return m_memory_manager; }
   PinMemoryManager *getPinMemoryManager() { return m_pin_memory_manager; }
   SyscallMdl *getSyscallMdl() { return m_syscall_model; }
   SyncClient *getSyncClient() { return m_sync_client; }
   ClockSkewMinimizationClient* getClockSkewMinimizationClient() { return m_clock_skew_minimization_client; }
   ShmemPerfModel* getShmemPerfModel() { return m_shmem_perf_model; }

   void updateInternalVariablesOnFrequencyChange(volatile float frequency);

   State getState();
   void setState(State core_state);

   void enablePerformanceModels();
   void disablePerformanceModels();
   void resetPerformanceModels();

private:
   
   class MemoryAccessStatus
   {
   public:
      MemoryAccessStatus(SInt32 access_id, UInt64 time,
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

   core_id_t m_core_id;
   MemoryManager *m_memory_manager;
   PinMemoryManager *m_pin_memory_manager;
   Network *m_network;
   PerformanceModel *m_performance_model;
   SyscallMdl *m_syscall_model;
   SyncClient *m_sync_client;
   ClockSkewMinimizationClient *m_clock_skew_minimization_client;
   ShmemPerfModel* m_shmem_perf_model;
  
   State m_core_state;
   
   // Memory Access Status
   UInt32 m_last_memory_access_id;
   std::map<UInt32, MemoryAccessStatus*> m_memory_access_status_map; 
   
   Lock m_core_state_lock;
   static Lock m_global_core_lock;

   PacketType getPktTypeFromUserNetType(carbon_network_t net_type);

   void continueMemoryAccess(MemoryAccessStatus& memory_access_status);
   void completeMemoryAccess(MemoryAccessStatus& memory_access_status);
};

#endif
