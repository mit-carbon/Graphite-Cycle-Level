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
   , m_terminated(false)
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
   // Register the sim thread
   Sim()->getCoreManager()->registerSimThread(sim_thread_id);

   // One EventQueueManager per SimThread
   EventQueueManager* event_queue_manager = Sim()->getEventManager()->getEventQueueManager(sim_thread_id);
 
   // Actual work gets done here
   while (!m_terminated)
   {
      // Wait for an event/net_packet
      LOG_PRINT("SimThread: processEvents()");
      event_queue_manager->processEvents();
   }

   LOG_PRINT("Sim thread exiting");
 
   // Unregister the sim thread from the core manager
   // Sim()->getCoreManager()->unregisterSimThread(sim_thread_id);
   // Subtract the number of active sim threads from the sim thread manager 
   Sim()->getSimThreadManager()->unregisterThread();
}

void SimThread::spawn()
{
   m_thread = Thread::create(this);
   m_thread->run();
}

void SimThread::terminate()
{
   m_terminated = true;
}

void SimThread::handleTerminationRequest(Event* event)
{
   UnstructuredBuffer* event_args = event->getArgs();
   SimThread* sim_thread;
   (*event_args) >> sim_thread;
   LOG_PRINT("Terminate(%p)", sim_thread);
   sim_thread->terminate();
}
