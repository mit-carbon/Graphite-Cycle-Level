#include "barrier_sync_client.h"
#include "barrier_sync_server.h"
#include "simulator.h"
#include "thread_manager.h"
#include "network.h"
#include "config.h"
#include "log.h"

BarrierSyncServer::BarrierSyncServer(Network &network, UnstructuredBuffer &recv_buff):
   m_network(network),
   m_recv_buff(recv_buff)
{
   m_thread_manager = Sim()->getThreadManager();
   try
   {
      m_barrier_interval = (UInt64) Sim()->getCfg()->getInt("clock_skew_minimization/barrier/quantum"); 
   }
   catch(...)
   {
      LOG_PRINT_ERROR("Error Reading 'clock_skew_minimization/barrier/quantum' from the config file");
   }

   m_next_barrier_time = m_barrier_interval;
   m_num_application_cores = Config::getSingleton()->getApplicationCores();
   m_local_clock_list.resize(m_num_application_cores);
   m_barrier_acquire_list.resize(m_num_application_cores);
   for (UInt32 i = 0; i < m_num_application_cores; i++)
   {
      m_local_clock_list[i] = 0;
      m_barrier_acquire_list[i] = false;
   }
}

BarrierSyncServer::~BarrierSyncServer()
{}

void
BarrierSyncServer::processSyncMsg(core_id_t core_id)
{
   barrierWait(core_id);
}

void
BarrierSyncServer::signal()
{
   if (isBarrierReached())
     barrierRelease(); 
}

void
BarrierSyncServer::barrierWait(core_id_t core_id)
{
   UInt64 time;
   m_recv_buff >> time;

   LOG_PRINT("Received 'SIM_BARRIER_WAIT' from Core(%i), Time(%llu)", core_id, time);

   LOG_ASSERT_ERROR(m_thread_manager->isThreadRunning(core_id) || m_thread_manager->isThreadInitializing(core_id), "Thread on core(%i) is not running or initializing at time(%llu)", core_id, time);

   if (time < m_next_barrier_time)
   {
      LOG_PRINT("Sent 'SIM_BARRIER_RELEASE' immediately time(%llu), m_next_barrier_time(%llu)", time, m_next_barrier_time);
      // LOG_PRINT_WARNING("core_id(%i), local_clock(%llu), m_next_barrier_time(%llu), m_barrier_interval(%llu)", core_id, time, m_next_barrier_time, m_barrier_interval);
      unsigned int reply = BarrierSyncClient::BARRIER_RELEASE;
      m_network.netSend(core_id, MCP_SYSTEM_RESPONSE_TYPE, (char*) &reply, sizeof(reply));
      return;
   }

   m_local_clock_list[core_id] = time;
   m_barrier_acquire_list[core_id] = true;
 
   signal(); 
}

bool
BarrierSyncServer::isBarrierReached()
{
   bool single_thread_barrier_reached = false;

   // Check if all threads have reached the barrier
   // All least one thread must have (sync_time > m_next_barrier_time)
   for (core_id_t core_id = 0; core_id < (core_id_t) m_num_application_cores; core_id++)
   {
      if (m_local_clock_list[core_id] < m_next_barrier_time)
      {
         if (m_thread_manager->isThreadRunning(core_id))
         {
            // Thread Running on this core has not reached the barrier
            // Wait for it to sync
            return false;
         }
      }
      else
      {
         LOG_ASSERT_ERROR(m_thread_manager->isThreadRunning(core_id) || m_thread_manager->isThreadInitializing(core_id), "Thread on core(%i) is not running or initializing at local_clock(%llu), m_next_barrier_time(%llu)", core_id, m_local_clock_list[core_id], m_next_barrier_time);

         // At least one thread has reached the barrier
         single_thread_barrier_reached = true;
      }
   }

   return single_thread_barrier_reached;
}

void
BarrierSyncServer::barrierRelease()
{
   LOG_PRINT("Sending 'BARRIER_RELEASE'");

   // All threads have reached the barrier
   // Advance m_next_barrier_time
   // Release the Barrier
   
   // If a thread cannot be resumed, we have to advance the sync 
   // time till a thread can be resumed. Then only, will we have 
   // forward progress

   bool thread_resumed = false;
   while (!thread_resumed)
   {
      m_next_barrier_time += m_barrier_interval;
      LOG_PRINT("m_next_barrier_time updated to (%llu)", m_next_barrier_time);

      for (core_id_t core_id = 0; core_id < (core_id_t) m_num_application_cores; core_id++)
      {
         if (m_local_clock_list[core_id] < m_next_barrier_time)
         {
            // Check if this core was running. If yes, send a message to that core
            if (m_barrier_acquire_list[core_id] == true)
            {
               LOG_ASSERT_ERROR(m_thread_manager->isThreadRunning(core_id) || m_thread_manager->isThreadInitializing(core_id), "(%i) has acquired barrier, local_clock(%i), m_next_barrier_time(%llu), but not initializing or running", core_id, m_local_clock_list[core_id], m_next_barrier_time);

               unsigned int reply = BarrierSyncClient::BARRIER_RELEASE;
               m_network.netSend(core_id, MCP_SYSTEM_RESPONSE_TYPE, (char*) &reply, sizeof(reply));

               m_barrier_acquire_list[core_id] = false;

               thread_resumed = true;
            }
         }
      }
   }
}
