#ifndef SIM_THREAD_H
#define SIM_THREAD_H

#include "thread.h"
#include "fixed_types.h"
#include "event.h"

class SimThread : public Runnable
{
public:
   SimThread();
   ~SimThread();

   void spawn();
   static void handleTerminationRequest(Event* event);

private:
   void run();
   void terminate();

   Thread *m_thread;
   volatile bool m_terminated;
};

#endif // SIM_THREAD_H
