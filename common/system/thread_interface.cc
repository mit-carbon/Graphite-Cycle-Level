#include "core.h"
#include "thread_interface.h"
#include "event.h"

ThreadInterface::ThreadInterface(Core* core)
   : _core(core)
{
}

ThreadInterface::~ThreadInterface()
{
   LOG_ASSERT_ERROR(_app_request_queue.empty(), "App Request Queue Size(%u)", _app_request_queue.size());
}

void
ThreadInterface::sendAppRequest(AppRequest app_request)
{
   LOG_PRINT("CoreID(%i): sendAppRequest(Type[%u])", _core->getId(), app_request.getType());

   if (Config::getSingleton()->getExecutionMode() == Config::NATIVE)
   {
      app_request.process(_core);
   }
   else // (Config::getSingleton()->getExecutionMode() != Config::NATIVE)
   {
      _lock.acquire();
      _app_request_queue.push(app_request);
      _lock.release();
      _request_semaphore.signal();
   }
}

AppRequest
ThreadInterface::recvAppRequest()
{
   _request_semaphore.wait();

   _lock.acquire();
   AppRequest app_request = _app_request_queue.front();
   _app_request_queue.pop();
   _lock.release();

   LOG_PRINT("CoreID(%i): recvAppRequest(Type[%u])", _core->getId(), app_request.getType());

   return app_request;
}

void
ThreadInterface::sendSimReply(UInt64 time, SimReply sim_reply)
{
   LOG_PRINT("Core(%i): sendSimReply(Time[%llu], SimReply[%llu])", _core->getId(), time, sim_reply);

   // Update the performance model time
   _core->getPerformanceModel()->updateTime(time);

   // Create an event that can accept further instructions
   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) << _core->getId();
   EventResumeThread* event = new EventResumeThread(time, event_args);
   Event::processInOrder(event, _core->getId(), EventQueue::ORDERED);

   // Signal the App thread
   _sim_reply = sim_reply;
   _reply_semaphore.signal();
}

void
ThreadInterface::sendSimInsReply(SimReply sim_reply)
{
   // Signal the App Thread
   _sim_reply = sim_reply;
   _reply_semaphore.signal();
}

SimReply
ThreadInterface::recvSimReply()
{
   // Wait for the Sim Thread
   _reply_semaphore.wait();

   LOG_PRINT("Core(%i): recvSimReply(%llu)", _core->getId(), _sim_reply);
   return _sim_reply;
}

void
ThreadInterface::iterate()
{
   bool cont = true;
   while (cont)
   {
      AppRequest app_request = recvAppRequest();
      cont = app_request.process(_core);
   }
}
