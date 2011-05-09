#include <vector>
using std::vector;

#include "sim_thread.h"
#include "core_manager.h"
#include "log.h"
#include "simulator.h"
#include "core.h"
#include "sim_thread_manager.h"
#include "event_manager.h"
#include "event_queue_manager.h"
#include "packet_type.h"

SimThread::SimThread()
   : m_thread(NULL)
{}

SimThread::~SimThread()
{
   delete m_thread;
}

void SimThread::run()
{
   LOG_PRINT("Sim thread starting...");

   // Get the sim thread id
   SInt32 sim_thread_id = Sim()->getSimThreadManager()->registerThread();
   // Get the core ids' associated with this sim thread id
   vector<core_id_t>& core_id_list = Sim()->getSimThreadManager()->getCoreIDListFromSimThreadID(sim_thread_id);
   // Get the cores associated with this sim thread id
   vector<Network*> network_list;
   for (vector<core_id_t>::iterator it = core_id_list.begin();
        it != core_id_list.end(); it++)
   {
      network_list.push_back(Sim()->getCoreManager()->getCoreFromID(*it)->getNetwork());
   }
   
   // Register the sim thread
   Sim()->getCoreManager()->registerSimThread(core_id_list.front());

   bool cont = true;
   // Turn off cont when we receive a quit message
   Network* net = network_list.front();
   net->registerCallback(SIM_THREAD_TERMINATE_THREADS,
                         terminateFunc,
                         &cont);

   // One EventQueueManager per SimThread
   EventQueueManager* event_queue_manager = Sim()->getEventManager()->getEventQueueManager(sim_thread_id);
   
   // Actual work gets done here
   while(cont)
   {
      // Wait for an event/net_packet
      LOG_PRINT("SimThread: processEvents()");
      event_queue_manager->processEvents();
   }

   Sim()->getSimThreadManager()->unregisterThread();

   LOG_PRINT("Sim thread exiting");
}

void SimThread::spawn()
{
   m_thread = Thread::create(this);
   m_thread->run();
}

void SimThread::terminateFunc(void *vp, NetPacket pkt)
{
   bool *pcont = (bool*) vp;
   *pcont = false;
}
