#include "sync_server.h"
#include "sync_client.h"
#include "simulator.h"
#include "thread_manager.h"

using namespace std;

// -- SimMutex -- //

SimMutex::SimMutex()
      : m_owner(NO_OWNER)
{ }

SimMutex::~SimMutex()
{
   assert(m_waiting.empty());
}

bool SimMutex::lock(core_id_t core_id)
{
   if (m_owner == NO_OWNER)
   {
      m_owner = core_id;
      return true;
   }
   else
   {
      Sim()->getThreadManager()->stallThread(core_id);
      m_waiting.push(core_id);
      return false;
   }
}

core_id_t SimMutex::unlock(core_id_t core_id)
{
   assert(m_owner == core_id);
   if (m_waiting.empty())
   {
      m_owner = NO_OWNER;
   }
   else
   {
      m_owner =  m_waiting.front();
      m_waiting.pop();
      Sim()->getThreadManager()->resumeThread(m_owner);
   }
   return m_owner;
}

// -- SimCond -- //
// FIXME: Currently, 'simulated times' are ignored in the synchronization constructs
SimCond::SimCond() {}
SimCond::~SimCond()
{
   assert(m_waiting.empty());
}

core_id_t SimCond::wait(core_id_t core_id, UInt64 time, StableIterator<SimMutex> & simMux)
{
   Sim()->getThreadManager()->stallThread(core_id);

   // If we don't have any later signals, then put this request in the queue
   m_waiting.push_back(CondWaiter(core_id, simMux, time));
   return simMux->unlock(core_id);
}

core_id_t SimCond::signal(core_id_t core_id, UInt64 time)
{
   // If there is a list of threads waiting, wake up one of them
   if (!m_waiting.empty())
   {
      CondWaiter woken = *(m_waiting.begin());
      m_waiting.erase(m_waiting.begin());

      Sim()->getThreadManager()->resumeThread(woken.m_core_id);

      if (woken.m_mutex->lock(woken.m_core_id))
      {
         // Woken up thread is able to grab lock immediately
         return woken.m_core_id;
      }
      else
      {
         // Woken up thread is *NOT* able to grab lock immediately
         return INVALID_CORE_ID;
      }
   }

   // There are *NO* threads waiting on the condition variable
   return INVALID_CORE_ID;
}

void SimCond::broadcast(core_id_t core_id, UInt64 time, WakeupList &woken_list)
{
   for (ThreadQueue::iterator i = m_waiting.begin(); i != m_waiting.end(); i++)
   {
      CondWaiter woken = *(i);

      Sim()->getThreadManager()->resumeThread(woken.m_core_id);

      if (woken.m_mutex->lock(woken.m_core_id))
      {
         // Woken up thread is able to grab lock immediately
         woken_list.push_back(woken.m_core_id);
      }
   }

   // All waiting threads have been woken up from the CondVar queue
   m_waiting.clear();
}

// -- SimBarrier -- //
SimBarrier::SimBarrier(UInt32 count)
      : m_count(count)
      , m_max_time(0)
{
}

SimBarrier::~SimBarrier()
{
   assert(m_waiting.empty());
}

void SimBarrier::wait(core_id_t core_id, UInt64 time, WakeupList &woken_list)
{
   m_waiting.push_back(core_id);

   Sim()->getThreadManager()->stallThread(core_id);

   assert(m_waiting.size() <= m_count);

   if (m_waiting.size() == 1)
      m_max_time = time;
   else if (time > m_max_time)
      m_max_time = time;

   // All threads have reached the barrier
   if (m_waiting.size() == m_count)
   {
      woken_list = m_waiting;

      for (WakeupList::iterator i = woken_list.begin(); i != woken_list.end(); i++)
      {
         // Resuming all the threads stalled at the barrier
         Sim()->getThreadManager()->resumeThread(*i);
      }
      m_waiting.clear();
   }
}

// -- SyncServer -- //

SyncServer::SyncServer(Network &network, UnstructuredBuffer &recv_buffer)
      : m_network(network),
      m_recv_buffer(recv_buffer)
{ }

SyncServer::~SyncServer()
{ }

void SyncServer::mutexInit(core_id_t core_id, UInt64 time)
{
   m_mutexes.push_back(SimMutex());
   UInt32 mux = (UInt32)m_mutexes.size()-1;

   NetPacket packet(time, MCP_RESPONSE_TYPE, m_network.getCore()->getId(), core_id,
         sizeof(mux), (void*) &mux);
   m_network.netSend(packet);
}

void SyncServer::mutexLock(core_id_t core_id, UInt64 time)
{
   carbon_mutex_t mux;
   m_recv_buffer >> mux;

   assert((size_t)mux < m_mutexes.size());

   SimMutex *psimmux = &m_mutexes[mux];

   if (psimmux->lock(core_id))
   {
      // notify the owner
      UInt32 reply = SyncClient::MUTEX_LOCK_RESPONSE;
      NetPacket packet(time, MCP_RESPONSE_TYPE, m_network.getCore()->getId(), core_id,
            sizeof(reply), (void*) &reply);
      m_network.netSend(packet);
   }
   else
   {
      // nothing...thread goes to sleep
   }
}

void SyncServer::mutexUnlock(core_id_t core_id, UInt64 time)
{
   carbon_mutex_t mux;
   m_recv_buffer >> mux;

   assert((size_t)mux < m_mutexes.size());

   SimMutex *psimmux = &m_mutexes[mux];

   core_id_t new_owner = psimmux->unlock(core_id);

   if (new_owner != SimMutex::NO_OWNER)
   {
      // wake up the new owner
      UInt32 reply = SyncClient::MUTEX_LOCK_RESPONSE;
      NetPacket packet(time, MCP_RESPONSE_TYPE, m_network.getCore()->getId(), new_owner,
            sizeof(reply), (void*) &reply);
      m_network.netSend(packet);
   }
   else
   {
      // nothing...
   }

   UInt32 reply = SyncClient::MUTEX_UNLOCK_RESPONSE;
   NetPacket packet(time, MCP_RESPONSE_TYPE, m_network.getCore()->getId(), core_id,
         sizeof(reply), (void*) &reply);
   m_network.netSend(packet);
}

// -- Condition Variable Stuffs -- //
void SyncServer::condInit(core_id_t core_id, UInt64 time)
{
   m_conds.push_back(SimCond());
   UInt32 cond = (UInt32)m_conds.size()-1;

   NetPacket packet(time, MCP_RESPONSE_TYPE, m_network.getCore()->getId(), core_id,
         sizeof(cond), (void*) &cond);
   m_network.netSend(packet);
}

void SyncServer::condWait(core_id_t core_id, UInt64 time)
{
   carbon_cond_t cond;
   carbon_mutex_t mux;
   m_recv_buffer >> cond;
   m_recv_buffer >> mux;

   assert((size_t)mux < m_mutexes.size());
   assert((size_t)cond < m_conds.size());

   SimCond *psimcond = &m_conds[cond];

   StableIterator<SimMutex> it(m_mutexes, mux);
   core_id_t new_mutex_owner = psimcond->wait(core_id, time, it);

   if (new_mutex_owner != SimMutex::NO_OWNER)
   {
      // wake up the new owner
      UInt32 reply = SyncClient::MUTEX_LOCK_RESPONSE;
      NetPacket packet(time, MCP_RESPONSE_TYPE, m_network.getCore()->getId(), new_mutex_owner,
            sizeof(reply), (void*) &reply);
      m_network.netSend(packet);
   }
}


void SyncServer::condSignal(core_id_t core_id, UInt64 time)
{
   carbon_cond_t cond;
   m_recv_buffer >> cond;

   assert((size_t)cond < m_conds.size());

   SimCond *psimcond = &m_conds[cond];

   core_id_t woken = psimcond->signal(core_id, time);

   if (woken != INVALID_CORE_ID)
   {
      // wake up the new owner
      // (note: COND_WAIT_RESPONSE == MUTEX_LOCK_RESPONSE, see header)
      UInt32 reply = SyncClient::MUTEX_LOCK_RESPONSE;
      NetPacket packet(time, MCP_RESPONSE_TYPE, m_network.getCore()->getId(), woken,
            sizeof(reply), (void*) &reply);
      m_network.netSend(packet);
   }
   else
   {
      // nothing...
   }

   // Alert the signaler
   UInt32 reply = SyncClient::COND_SIGNAL_RESPONSE;
   NetPacket packet(time, MCP_RESPONSE_TYPE, m_network.getCore()->getId(), core_id,
         sizeof(reply), (void*) &reply);
   m_network.netSend(packet);
}

void SyncServer::condBroadcast(core_id_t core_id, UInt64 time)
{
   carbon_cond_t cond;
   m_recv_buffer >> cond;

   assert((size_t)cond < m_conds.size());

   SimCond *psimcond = &m_conds[cond];

   SimCond::WakeupList woken_list;
   psimcond->broadcast(core_id, time, woken_list);

   for (SimCond::WakeupList::iterator it = woken_list.begin(); it != woken_list.end(); it++)
   {
      assert(*it != INVALID_CORE_ID);

      // wake up the new owner
      // (note: COND_WAIT_RESPONSE == MUTEX_LOCK_RESPONSE, see header)
      UInt32 reply = SyncClient::MUTEX_LOCK_RESPONSE;
      NetPacket packet(time, MCP_RESPONSE_TYPE, m_network.getCore()->getId(), *it,
            sizeof(reply), (void*) &reply);
      m_network.netSend(packet);
   }

   // Alert the signaler
   UInt32 reply = SyncClient::COND_BROADCAST_RESPONSE;
   NetPacket packet(time, MCP_RESPONSE_TYPE, m_network.getCore()->getId(), core_id,
         sizeof(reply), (void*) &reply);
   m_network.netSend(packet);
}

void SyncServer::barrierInit(core_id_t core_id, UInt64 time)
{
   UInt32 count;
   m_recv_buffer >> count;

   m_barriers.push_back(SimBarrier(count));
   UInt32 barrier = (UInt32)m_barriers.size()-1;

   NetPacket packet(time, MCP_RESPONSE_TYPE, m_network.getCore()->getId(), core_id,
         sizeof(barrier), (void*) &barrier);
   m_network.netSend(packet);
}

void SyncServer::barrierWait(core_id_t core_id, UInt64 time)
{
   carbon_barrier_t barrier;
   m_recv_buffer >> barrier;

   LOG_ASSERT_ERROR(barrier < (core_id_t) m_barriers.size(),
         "barrier = %i, m_barriers.size()= %u", barrier, m_barriers.size());

   SimBarrier *psimbarrier = &m_barriers[barrier];

   SimBarrier::WakeupList woken_list;
   psimbarrier->wait(core_id, time, woken_list);

   UInt64 max_time = psimbarrier->getMaxTime();

   for (SimBarrier::WakeupList::iterator it = woken_list.begin(); it != woken_list.end(); it++)
   {
      assert(*it != INVALID_CORE_ID);
      UInt32 reply = SyncClient::BARRIER_WAIT_RESPONSE;
      NetPacket packet(max_time, MCP_RESPONSE_TYPE, m_network.getCore()->getId(), *it,
            sizeof(reply), (void*) &reply);
      m_network.netSend(packet);
   }
}
