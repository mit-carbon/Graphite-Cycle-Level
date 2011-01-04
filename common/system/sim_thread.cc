#include "sim_thread.h"
#include "core_manager.h"
#include "log.h"
#include "simulator.h"
#include "core.h"
#include "sim_thread_manager.h"
#include "event_manager.h"
#include "event_heap.h"

SimThread::SimThread()
   : m_thread(NULL)
{
}

SimThread::~SimThread()
{
   delete m_thread;
}

void SimThread::run()
{
   __attribute__((__unused__)) core_id_t core_id = Sim()->getCoreManager()->registerSimThread();

   LOG_PRINT("Sim thread starting...");

   SInt32 sim_thread_id = Sim()->getSimThreadManager()->registerSimThread();

   EventHeap* event_heap = Sim()->getEventManager()->getEventHeapFromSimThreadId(sim_thread_id);

   // Actual work gets done here
   while (Sim()->getSimThreadManager()->isSimulationRunning())
      event_heap->poll();

   Sim()->getSimThreadManager()->unregisterSimThread();

   LOG_PRINT("Sim thread exiting");
}

void SimThread::spawn()
{
   m_thread = Thread::create(this);
   m_thread->run();
}
