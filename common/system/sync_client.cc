#include "sync_client.h"
#include "network.h"
#include "core.h"
#include "packetize.h"
#include "mcp.h"
#include "clock_converter.h"
#include "fxsupport.h"

#include <iostream>

using namespace std;

SyncClient::SyncClient(Core *core)
      : m_core(core)
      , m_network(core->getNetwork())
{
}

SyncClient::~SyncClient()
{
}

void SyncClient::mutexInit(carbon_mutex_t *mux)
{
   // Reset the buffers for the new transmission
   m_recv_buff.clear();
   m_send_buff.clear();

   int msg_type = MCP_MESSAGE_MUTEX_INIT;

   m_send_buff << msg_type;

   m_network->netSend(Config::getSingleton()->getMCPCoreNum(), MCP_REQUEST_TYPE,
         m_send_buff.getBuffer(), m_send_buff.size());

   NetPacket* recv_pkt;
   recv_pkt = m_network->netRecv(Config::getSingleton()->getMCPCoreNum(), MCP_RESPONSE_TYPE);
   assert(recv_pkt->length == sizeof(carbon_mutex_t));

   *mux = *((carbon_mutex_t*)recv_pkt->data);

   recv_pkt->release();
}

void SyncClient::mutexLock(carbon_mutex_t *mux)
{
   // Save/Restore Floating Point state
   FloatingPointHandler floating_point_handler;

   // Reset the buffers for the new transmission
   m_recv_buff.clear();
   m_send_buff.clear();

   int msg_type = MCP_MESSAGE_MUTEX_LOCK;

   m_send_buff << msg_type << *mux;

   m_network->netSend(Config::getSingleton()->getMCPCoreNum(), MCP_REQUEST_TYPE,
         m_send_buff.getBuffer(), m_send_buff.size());

   // Set the CoreState to 'STALLED'
   m_network->getCore()->setState(Core::STALLED);

   NetPacket* recv_pkt;
   recv_pkt = m_network->netRecv(Config::getSingleton()->getMCPCoreNum(), MCP_RESPONSE_TYPE);
   assert(recv_pkt->length == sizeof(unsigned int));

   // Set the CoreState to 'RUNNING'
   m_network->getCore()->setState(Core::WAKING_UP);

   unsigned int dummy;
   m_recv_buff << make_pair(recv_pkt->data, recv_pkt->length);
   m_recv_buff >> dummy;
   assert(dummy == MUTEX_LOCK_RESPONSE);

   recv_pkt->release();
}

void SyncClient::mutexUnlock(carbon_mutex_t *mux)
{
   // Save/Restore Floating Point state
   FloatingPointHandler floating_point_handler;

   // Reset the buffers for the new transmission
   m_recv_buff.clear();
   m_send_buff.clear();

   int msg_type = MCP_MESSAGE_MUTEX_UNLOCK;

   m_send_buff << msg_type << *mux;

   m_network->netSend(Config::getSingleton()->getMCPCoreNum(), MCP_REQUEST_TYPE,
         m_send_buff.getBuffer(), m_send_buff.size());

   NetPacket* recv_pkt;
   recv_pkt = m_network->netRecv(Config::getSingleton()->getMCPCoreNum(), MCP_RESPONSE_TYPE);
   assert(recv_pkt->length == sizeof(unsigned int));

   unsigned int dummy;
   m_recv_buff << make_pair(recv_pkt->data, recv_pkt->length);
   m_recv_buff >> dummy;
   assert(dummy == MUTEX_UNLOCK_RESPONSE);

   recv_pkt->release();
}

void SyncClient::condInit(carbon_cond_t *cond)
{
   // Save/Restore Floating Point state
   FloatingPointHandler floating_point_handler;

   // Reset the buffers for the new transmission
   m_recv_buff.clear();
   m_send_buff.clear();

   int msg_type = MCP_MESSAGE_COND_INIT;

   m_send_buff << msg_type << *cond;

   m_network->netSend(Config::getSingleton()->getMCPCoreNum(), MCP_REQUEST_TYPE,
         m_send_buff.getBuffer(), m_send_buff.size());

   NetPacket* recv_pkt;
   recv_pkt = m_network->netRecv(Config::getSingleton()->getMCPCoreNum(), MCP_RESPONSE_TYPE);
   assert(recv_pkt->length == sizeof(carbon_cond_t));

   *cond = *((carbon_cond_t*)recv_pkt->data);

   recv_pkt->release();
}

void SyncClient::condWait(carbon_cond_t *cond, carbon_mutex_t *mux)
{
   // Save/Restore Floating Point state
   FloatingPointHandler floating_point_handler;

   // Reset the buffers for the new transmission
   m_recv_buff.clear();
   m_send_buff.clear();

   int msg_type = MCP_MESSAGE_COND_WAIT;

   m_send_buff << msg_type << *cond << *mux;

   m_network->netSend(Config::getSingleton()->getMCPCoreNum(), MCP_REQUEST_TYPE,
         m_send_buff.getBuffer(), m_send_buff.size());

   // Set the CoreState to 'STALLED'
   m_network->getCore()->setState(Core::STALLED);

   NetPacket* recv_pkt;
   recv_pkt = m_network->netRecv(Config::getSingleton()->getMCPCoreNum(), MCP_RESPONSE_TYPE);
   assert(recv_pkt->length == sizeof(unsigned int));

   // Set the CoreState to 'RUNNING'
   m_network->getCore()->setState(Core::WAKING_UP);

   unsigned int dummy;
   m_recv_buff << make_pair(recv_pkt->data, recv_pkt->length);
   m_recv_buff >> dummy;
   assert(dummy == COND_WAIT_RESPONSE);

   recv_pkt->release();
}

void SyncClient::condSignal(carbon_cond_t *cond)
{
   // Save/Restore Floating Point state
   FloatingPointHandler floating_point_handler;

   // Reset the buffers for the new transmission
   m_recv_buff.clear();
   m_send_buff.clear();

   int msg_type = MCP_MESSAGE_COND_SIGNAL;

   m_send_buff << msg_type << *cond;

   m_network->netSend(Config::getSingleton()->getMCPCoreNum(), MCP_REQUEST_TYPE,
         m_send_buff.getBuffer(), m_send_buff.size());

   NetPacket* recv_pkt;
   recv_pkt = m_network->netRecv(Config::getSingleton()->getMCPCoreNum(), MCP_RESPONSE_TYPE);
   assert(recv_pkt->length == sizeof(unsigned int));

   unsigned int dummy;
   m_recv_buff << make_pair(recv_pkt->data, recv_pkt->length);
   m_recv_buff >> dummy;
   assert(dummy == COND_SIGNAL_RESPONSE);

   recv_pkt->release();
}

void SyncClient::condBroadcast(carbon_cond_t *cond)
{
   // Save/Restore Floating Point state
   FloatingPointHandler floating_point_handler;

   // Reset the buffers for the new transmission
   m_recv_buff.clear();
   m_send_buff.clear();

   int msg_type = MCP_MESSAGE_COND_BROADCAST;

   m_send_buff << msg_type << *cond;

   m_network->netSend(Config::getSingleton()->getMCPCoreNum(), MCP_REQUEST_TYPE,
         m_send_buff.getBuffer(), m_send_buff.size());

   NetPacket* recv_pkt;
   recv_pkt = m_network->netRecv(Config::getSingleton()->getMCPCoreNum(), MCP_RESPONSE_TYPE);
   assert(recv_pkt->length == sizeof(unsigned int));

   unsigned int dummy;
   m_recv_buff << make_pair(recv_pkt->data, recv_pkt->length);
   m_recv_buff >> dummy;
   assert(dummy == COND_BROADCAST_RESPONSE);

   recv_pkt->release();
}

void SyncClient::barrierInit(carbon_barrier_t *barrier, UInt32 count)
{
   // Save/Restore Floating Point state
   FloatingPointHandler floating_point_handler;

   // Reset the buffers for the new transmission
   m_recv_buff.clear();
   m_send_buff.clear();

   int msg_type = MCP_MESSAGE_BARRIER_INIT;

   m_send_buff << msg_type << count;

   m_network->netSend(Config::getSingleton()->getMCPCoreNum(), MCP_REQUEST_TYPE,
         m_send_buff.getBuffer(), m_send_buff.size());

   NetPacket* recv_pkt;
   recv_pkt = m_network->netRecv(Config::getSingleton()->getMCPCoreNum(), MCP_RESPONSE_TYPE);
   assert(recv_pkt->length == sizeof(carbon_barrier_t));

   *barrier = *((carbon_barrier_t*)recv_pkt->data);

   recv_pkt->release();
}

void SyncClient::barrierWait(carbon_barrier_t *barrier)
{
   // Save/Restore Floating Point state
   FloatingPointHandler floating_point_handler;

   // Reset the buffers for the new transmission
   m_recv_buff.clear();
   m_send_buff.clear();

   int msg_type = MCP_MESSAGE_BARRIER_WAIT;

   m_send_buff << msg_type << *barrier;

   m_network->netSend(Config::getSingleton()->getMCPCoreNum(), MCP_REQUEST_TYPE,
         m_send_buff.getBuffer(), m_send_buff.size());

   // Set the CoreState to 'STALLED'
   m_network->getCore()->setState(Core::STALLED);

   NetPacket* recv_pkt;
   recv_pkt = m_network->netRecv(Config::getSingleton()->getMCPCoreNum(), MCP_RESPONSE_TYPE);
   assert(recv_pkt->length == sizeof(unsigned int));

   // Set the CoreState to 'RUNNING'
   m_network->getCore()->setState(Core::WAKING_UP);

   unsigned int dummy;
   m_recv_buff << make_pair(recv_pkt->data, recv_pkt->length);
   m_recv_buff >> dummy;
   assert(dummy == BARRIER_WAIT_RESPONSE);

   recv_pkt->release();
}
