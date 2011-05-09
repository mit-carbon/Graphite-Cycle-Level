#pragma once

#include <queue>
using std::queue;
#include "lock.h"
#include "semaphore.h"
#include "app_request.h"

class Core;

class ThreadInterface
{
public:
   ThreadInterface(Core* core);
   ~ThreadInterface();

   // Spin (in sim thread) waiting for requests from the app thread
   void iterate();
   // Send request from app thread to sim thread
   void sendAppRequest(AppRequest app_request);
   // Wait (in app thread) expecting a reply from the sim thread
   SimReply recvSimReply();
   // Send reply from sim thread to app thread
   void sendSimReply(UInt64 time, SimReply sim_reply = 0);
   // Send reply from sim thread to app thread
   void sendSimInsReply(SimReply sim_reply);

private:
   queue<AppRequest> _app_request_queue;
   SimReply _sim_reply;

   Core* _core;

   // Synchronization
   Lock _lock;
   Semaphore _request_semaphore;
   Semaphore _reply_semaphore;
   
   // Wait (in sim thread) expecting a request from the app thread
   AppRequest recvAppRequest();
};
