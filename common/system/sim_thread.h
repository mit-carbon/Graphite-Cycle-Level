#ifndef SIM_THREAD_H
#define SIM_THREAD_H

#include "thread.h"
#include "fixed_types.h"

class SimThread : public Runnable
{
public:
   SimThread();
   ~SimThread();

   void spawn();

private:
   void run();

   Thread *m_thread;
};

#endif // SIM_THREAD_H
