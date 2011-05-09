#pragma once

#include "packetize.h"

class Core;

class AppRequest
{
public:
   enum Type
   {
      HANDLE_INSTRUCTION = 0,
      EMULATE_ROUTINE,
      HANDLE_SYSCALL,
      PROCESS_THREAD_EXIT,
      NUM_TYPES
   };

   AppRequest(Type type, UnstructuredBuffer* request = NULL)
      : _type(type), _request(request) {}
   ~AppRequest() {}

   bool process(Core* core);
   Type getType() { return _type; }
   void* getRequest() { return _request; }

private: 
   Type _type;
   UnstructuredBuffer* _request;
};

typedef IntPtr SimReply;
