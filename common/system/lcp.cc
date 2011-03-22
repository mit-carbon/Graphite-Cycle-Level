#include "lcp.h"
#include "simulator.h"
#include "core.h"
#include "message_types.h"
#include "thread_manager.h"
#include "clock_skew_minimization_object.h"
#include "event.h"

#include "log.h"

// -- general LCP functionality

LCP::LCP()
   : m_proc_num(Config::getSingleton()->getCurrentProcessNum())
   , m_transport(Transport::getSingleton()->getGlobalNode())
   , m_finished(false)
{
}

LCP::~LCP()
{
}

void LCP::run()
{
   LOG_PRINT("LCP started.");

   while (!m_finished)
   {
      processMessage();
   }
}

void LCP::processMessage()
{
   pair<Byte*,SInt32> msg_tag_pair = m_transport->recv();
   Byte *msg = msg_tag_pair.first;
   SInt32 tag = msg_tag_pair.second;
   assert(tag == -1);

   SInt32 *msg_type = (SInt32*) msg;

   LOG_PRINT("Received message type: %d", *msg_type);

   Byte *data = msg + sizeof(SInt32);

   switch (*msg_type)
   {
   case LCP_MESSAGE_QUIT:
      LOG_PRINT("Received quit message.");
      m_finished = true;
      break;

   case LCP_MESSAGE_COMMID_UPDATE:
      updateCommId(data);
      break;

   case LCP_MESSAGE_SIMULATOR_FINISHED:
      Sim()->handleFinish();
      break;

   case LCP_MESSAGE_SIMULATOR_FINISHED_ACK:
      Sim()->deallocateProcess();
      break;

   case LCP_MESSAGE_THREAD_SPAWN_REQUEST_FROM_MASTER:
      Sim()->getThreadManager()->slaveSpawnThread((ThreadSpawnRequest*)msg);
      break;
      
   case LCP_MESSAGE_QUIT_THREAD_SPAWNER:
      Sim()->getThreadManager()->slaveTerminateThreadSpawner();
      break;

   case LCP_MESSAGE_QUIT_THREAD_SPAWNER_ACK:
      Sim()->getThreadManager()->updateTerminateThreadSpawner();
      break;

   case LCP_MESSAGE_CLOCK_SKEW_MINIMIZATION:
      assert (Sim()->getClockSkewMinimizationManager());
      Sim()->getClockSkewMinimizationManager()->processSyncMsg(data);

   default:
      LOG_ASSERT_ERROR(false, "Unexpected message type: %d.", *msg_type);
      break;
   }

   delete [] msg;
}

void LCP::finish()
{
   LOG_PRINT("Send LCP quit message");

   SInt32 msg_type = LCP_MESSAGE_QUIT;

   m_transport->globalSend(m_proc_num,
                           &msg_type,
                           sizeof(msg_type));

   while (!m_finished)
      sched_yield();

   LOG_PRINT("LCP finished.");
}

// -- functions for specific tasks

struct CommMapUpdate
{
   SInt32 comm_id;
   core_id_t core_id;
};

void LCP::updateCommId(void *vp)
{
   CommMapUpdate *update = (CommMapUpdate*)vp;

   LOG_PRINT("Initializing comm_id: %d to core_id: %d", update->comm_id, update->core_id);
   Config::getSingleton()->updateCommToCoreMap(update->comm_id, update->core_id);

   NetPacket* ack = new NetPacket(/*time*/ 0,
                                  /*type*/ LCP_COMM_ID_UPDATE_REPLY,
                                  /*sender*/ 0, // doesn't matter ; see core_manager.cc
                                  /*receiver*/ update->core_id,
                                  /*length*/ 0,
                                  /*data*/ NULL);
  
   UnstructuredBuffer event_args;
   event_args << update->core_id << ack;
   EventNetwork* event = new EventNetwork(0, event_args);
   Event::processInOrder(event, update->core_id, EventQueue::UNORDERED);
}
