#include <string>
#include <map>
using namespace std;

#include "routine_replace.h"
#include "simulator.h"
#include "core_manager.h"
#include "core.h"
#include "routine_manager.h"
#include "thread_manager.h"
#include "log.h"

// The Pintool can easily read from application memory, so
// we dont need to explicitly initialize stuff and do a special ret
void routineCallback(RTN rtn, void* v)
{
   string rtn_name = RTN_Name(rtn);

   // Enable Models
   if (rtn_name == "CarbonEnableModels")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(void),
            CALLINGSTD_DEFAULT,
            "CarbonEnableModels",
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CarbonEnableModels),
            IARG_PROTOTYPE, proto,
            IARG_END);
   }
 
   // Disable Models
   if (rtn_name == "CarbonDisableModels")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(void),
            CALLINGSTD_DEFAULT,
            "CarbonDisableModels",
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CarbonDisableModels),
            IARG_PROTOTYPE, proto,
            IARG_END);
   }

   // _start
   if (rtn_name == "_start")
   {
      RTN_Open(rtn);

      RTN_InsertCall(rtn, IPOINT_BEFORE,
            AFUNPTR(Simulator::disablePerformanceModels),
            IARG_END);

      RTN_Close(rtn);
   }

   // main
   if (rtn_name == "main")
   {
      RTN_Open(rtn);

      // Before main()
      if (Sim()->getCfg()->getBool("general/enable_models_at_startup",true))
      {
         RTN_InsertCall(rtn, IPOINT_BEFORE,
               AFUNPTR(Simulator::enablePerformanceModels),
               IARG_END);
      }

      RTN_InsertCall(rtn, IPOINT_BEFORE,
            AFUNPTR(CarbonInitModels),
            IARG_END);

      // After main()
      RTN_InsertCall(rtn, IPOINT_AFTER,
            AFUNPTR(Simulator::disablePerformanceModels),
            IARG_END);

      RTN_Close(rtn);
   }

   // CarbonStartSim() and CarbonStopSim()
   if (rtn_name == "CarbonStartSim")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(SInt32),
            CALLINGSTD_DEFAULT,
            "CarbonStartSim",
            PIN_PARG(int),
            PIN_PARG(char**),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(nullFunction),
            IARG_PROTOTYPE, proto,
            IARG_END);
   }
   else if (rtn_name == "CarbonStopSim")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(void),
            CALLINGSTD_DEFAULT,
            "CarbonStopSim",
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(nullFunction),
            IARG_PROTOTYPE, proto,
            IARG_END);
   }

   // Thread Creation
   else if (rtn_name == "CarbonSpawnThread")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(core_id_t),
            CALLINGSTD_DEFAULT,
            "CarbonSpawnThread",
            PIN_PARG(thread_func_t),
            PIN_PARG(void*),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(emulateCarbonSpawnThread),
            IARG_PROTOTYPE, proto,
            IARG_CONTEXT,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_END);
   }
   else if (rtn_name.find("pthread_create") != string::npos)
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(int),
            CALLINGSTD_DEFAULT,
            "pthread_create",
            PIN_PARG(pthread_t*),
            PIN_PARG(pthread_attr_t*),
            PIN_PARG(void* (*)(void*)),
            PIN_PARG(void*),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(emulatePthreadCreate),
            IARG_PROTOTYPE, proto,
            IARG_CONTEXT,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
            IARG_END);
   }
   // Thread Joining
   else if (rtn_name == "CarbonJoinThread")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(void),
            CALLINGSTD_DEFAULT,
            "CarbonJoinThread",
            PIN_PARG(core_id_t),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(emulateCarbonJoinThread),
            IARG_PROTOTYPE, proto,
            IARG_CONTEXT,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);
   }
   else if (rtn_name.find("pthread_join") != string::npos)
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(int),
            CALLINGSTD_DEFAULT,
            "pthread_join",
            PIN_PARG(pthread_t),
            PIN_PARG(void**),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(emulatePthreadJoin),
            IARG_PROTOTYPE, proto,
            IARG_CONTEXT,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_END);
   }
   // Synchronization
   else if (rtn_name == "CarbonMutexInit")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(void),
            CALLINGSTD_DEFAULT,
            "CarbonMutexInit",
            PIN_PARG(carbon_mutex_t*),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CarbonMutexInit),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);
   }
   else if (rtn_name == "CarbonMutexLock")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(void),
            CALLINGSTD_DEFAULT,
            "CarbonMutexLock",
            PIN_PARG(carbon_mutex_t*),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CarbonMutexLock),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);
   }
   else if (rtn_name == "CarbonMutexUnlock")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(void),
            CALLINGSTD_DEFAULT,
            "CarbonMutexUnlock",
            PIN_PARG(carbon_mutex_t*),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CarbonMutexUnlock),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);
   }
   else if (rtn_name == "CarbonCondInit")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(void),
            CALLINGSTD_DEFAULT,
            "CarbonCondInit",
            PIN_PARG(carbon_cond_t*),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CarbonCondInit),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);
   }
   else if (rtn_name == "CarbonCondWait")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(void),
            CALLINGSTD_DEFAULT,
            "CarbonCondWait",
            PIN_PARG(carbon_cond_t*),
            PIN_PARG(carbon_mutex_t*),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CarbonCondWait),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_END);
   }
   else if (rtn_name == "CarbonCondSignal")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(void),
            CALLINGSTD_DEFAULT,
            "CarbonCondSignal",
            PIN_PARG(carbon_cond_t*),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CarbonCondSignal),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);
   }
   else if (rtn_name == "CarbonCondBroadcast")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(void),
            CALLINGSTD_DEFAULT,
            "CarbonCondBroadcast",
            PIN_PARG(carbon_cond_t*),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CarbonCondBroadcast),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);
   }
   else if (rtn_name == "CarbonBarrierInit")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(void),
            CALLINGSTD_DEFAULT,
            "CarbonBarrierInit",
            PIN_PARG(carbon_barrier_t*),
            PIN_PARG(unsigned int),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CarbonBarrierInit),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_END);
   }
   else if (rtn_name == "CarbonBarrierWait")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(void),
            CALLINGSTD_DEFAULT,
            "CarbonBarrierWait",
            PIN_PARG(carbon_barrier_t*),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CarbonBarrierWait),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);
   }
   
   // CAPI Functions
   else if (rtn_name == "CAPI_Initialize")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(CAPI_return_t),
            CALLINGSTD_DEFAULT,
            "CAPI_Initialize",
            PIN_PARG(int),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CAPI_Initialize),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);
   }
   else if (rtn_name == "CAPI_rank")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(CAPI_return_t),
            CALLINGSTD_DEFAULT,
            "CAPI_rank",
            PIN_PARG(int*),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CAPI_rank),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);
   }
   else if (rtn_name == "CAPI_message_send_w")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(CAPI_return_t),
            CALLINGSTD_DEFAULT,
            "CAPI_message_send_w",
            PIN_PARG(CAPI_endpoint_t),
            PIN_PARG(CAPI_endpoint_t),
            PIN_PARG(char*),
            PIN_PARG(int),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CAPI_message_send_w),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
            IARG_END);
   }
   else if (rtn_name == "CAPI_message_receive_w")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(CAPI_return_t),
            CALLINGSTD_DEFAULT,
            "CAPI_message_receive_w",
            PIN_PARG(CAPI_endpoint_t),
            PIN_PARG(CAPI_endpoint_t),
            PIN_PARG(char*),
            PIN_PARG(int),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CAPI_message_receive_w),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
            IARG_END);
   }
   else if (rtn_name == "CAPI_message_send_w_ex")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(CAPI_return_t),
            CALLINGSTD_DEFAULT,
            "CAPI_message_send_w_ex",
            PIN_PARG(CAPI_endpoint_t),
            PIN_PARG(CAPI_endpoint_t),
            PIN_PARG(char*),
            PIN_PARG(int),
            PIN_PARG(UInt32),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CAPI_message_send_w_ex),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4,
            IARG_END);
   }
   else if (rtn_name == "CAPI_message_receive_w_ex")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(CAPI_return_t),
            CALLINGSTD_DEFAULT,
            "CAPI_message_receive_w_ex",
            PIN_PARG(CAPI_endpoint_t),
            PIN_PARG(CAPI_endpoint_t),
            PIN_PARG(char*),
            PIN_PARG(int),
            PIN_PARG(UInt32),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CAPI_message_receive_w_ex),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4,
            IARG_END);
   }

   // Getting Simulated Time
   else if (rtn_name == "CarbonGetTime")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(UInt64),
            CALLINGSTD_DEFAULT,
            "CarbonGetTime",
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CarbonGetTime),
            IARG_PROTOTYPE, proto,
            IARG_END);
   }

   // Frequency Scaling Functions
   else if (rtn_name == "CarbonGetCoreFrequency")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(void),
            CALLINGSTD_DEFAULT,
            "CarbonGetCoreFrequency",
            PIN_PARG(float*),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CarbonGetCoreFrequency),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);
   }
   else if (rtn_name == "CarbonSetCoreFrequency")
   {
      PROTO proto = PROTO_Allocate(PIN_PARG(void),
            CALLINGSTD_DEFAULT,
            "CarbonSetCoreFrequency",
            PIN_PARG(float*),
            PIN_PARG_END());

      RTN_ReplaceSignature(rtn,
            AFUNPTR(CarbonSetCoreFrequency),
            IARG_PROTOTYPE, proto,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);
   }
}

AFUNPTR getFunptr(CONTEXT* context, string func_name)
{
   IntPtr reg_inst_ptr = PIN_GetContextReg(context, REG_INST_PTR);

   PIN_LockClient();
   IMG img = IMG_FindByAddress(reg_inst_ptr);
   RTN rtn = RTN_FindByName(img, func_name.c_str());
   PIN_UnlockClient();
   
   return RTN_Funptr(rtn);
}

core_id_t emulateCarbonSpawnThread(CONTEXT* context,
                                   thread_func_t thread_func,
                                   void* arg)
{
   LOG_PRINT("Entering emulateCarbonSpawnThread(%p, %p)", thread_func, arg);
 
   // Call CARBON_SPAWN_THREAD to get a Core
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_SPAWN_THREAD << thread_func << arg;
   core_id_t spawned_core_id = (core_id_t) emulateRoutine(routine_info);

   // Find and call the pthread_create function 
   AFUNPTR pthread_create_func = getFunptr(context, "pthread_create");
   LOG_ASSERT_ERROR(pthread_create_func != NULL, "Could not find pthread_create");

   int ret;
   pthread_t* thread_ptr = new pthread_t;
   PIN_CallApplicationFunction(context, PIN_ThreadId(),
         CALLINGSTD_DEFAULT,
         pthread_create_func,
         PIN_PARG(int), &ret,
         PIN_PARG(pthread_t*), thread_ptr,
         PIN_PARG(pthread_attr_t*), NULL,
         PIN_PARG(void* (*)(void*)), thread_func,
         PIN_PARG(void*), arg,
         PIN_PARG_END());

   LOG_ASSERT_ERROR(ret == 0, "pthread_create() returned(%i)", ret);
   
   // Insert the Core ID -> pthread_t* Mapping
   Sim()->getThreadManager()->insertCoreIDToThreadMapping(spawned_core_id, thread_ptr);
   
   return spawned_core_id;
}

int emulatePthreadCreate(CONTEXT* context,
                         pthread_t* thread_ptr,
                         pthread_attr_t* attr,
                         thread_func_t thread_func,
                         void* arg)
{
   // Call CARBON_SPAWN_THREAD to get a Core
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_SPAWN_THREAD << thread_func << arg;
   core_id_t spawned_core_id = (core_id_t) emulateRoutine(routine_info);
  
   // Find and call the pthread_create function 
   AFUNPTR pthread_create_func = getFunptr(context, "pthread_create");
   LOG_ASSERT_ERROR(pthread_create_func != NULL, "Could not find pthread_create");

   int ret;
   PIN_CallApplicationFunction(context, PIN_ThreadId(),
         CALLINGSTD_DEFAULT,
         pthread_create_func,
         PIN_PARG(int), &ret,
         PIN_PARG(pthread_t*), thread_ptr,
         PIN_PARG(pthread_attr_t*), attr,
         PIN_PARG(void* (*)(void*)), thread_func,
         PIN_PARG(void*), arg,
         PIN_PARG_END());

   LOG_ASSERT_ERROR(ret == 0, "pthread_create() returned(%i)", ret);

   // Insert the Core ID -> pthread_t* Mapping
   Sim()->getThreadManager()->insertCoreIDToThreadMapping(spawned_core_id, thread_ptr);

   return ret;
}

void emulateCarbonJoinThread(CONTEXT* context,
                             core_id_t join_core_id)
{
   // Get pthread_t* from join_core_id
   pthread_t* thread_ptr = Sim()->getThreadManager()->getThreadFromCoreID(join_core_id);
   assert(thread_ptr);
   // Erase the Core Id to pthread_t* mapping
   Sim()->getThreadManager()->eraseCoreIDToThreadMapping(join_core_id, thread_ptr);

   // Call CARBON_JOIN_THREAD to join the core
   LOG_PRINT("Starting emulateCarbonJoinThread: Thread_ptr(%p), join_core_id(%i)", thread_ptr, join_core_id);
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_JOIN_THREAD << join_core_id;
   emulateRoutine(routine_info);

   // Call the pthread_join function
   AFUNPTR pthread_join_func = getFunptr(context, "pthread_join");
   LOG_ASSERT_ERROR(pthread_join_func != NULL, "Could not find pthread_join");

   int ret;
   PIN_CallApplicationFunction(context, PIN_ThreadId(),
         CALLINGSTD_DEFAULT,
         pthread_join_func,
         PIN_PARG(int), &ret,
         PIN_PARG(pthread_t), *thread_ptr,
         PIN_PARG(void**), NULL,
         PIN_PARG_END());
   LOG_ASSERT_ERROR(ret == 0, "pthread_join() returned(%i)", ret);
   
   LOG_PRINT("Finished emulateCarbonJoinThread: Thread_ptr(%p), join_core_id(%i)", thread_ptr, join_core_id);
   
   // Delete the thread descriptor
   delete thread_ptr;
}

int emulatePthreadJoin(CONTEXT* context,
                       pthread_t thread,
                       void** thead_return)
{
   // Get the Join Core Id
   core_id_t join_core_id = Sim()->getThreadManager()->getCoreIDFromThread(&thread);
   assert(join_core_id != INVALID_CORE_ID);
   // Erase the Core Id -> pthread_t* mapping
   Sim()->getThreadManager()->eraseCoreIDToThreadMapping(join_core_id, &thread);
  
   LOG_PRINT("Joining Thread_ptr(%p), join_core_id(%i)", &thread, join_core_id);

   // Call the CARBON_JOIN_THREAD function
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CARBON_JOIN_THREAD << join_core_id;
   emulateRoutine(routine_info);

   // Call the pthread_join function
   AFUNPTR pthread_join_func = getFunptr(context, "pthread_join");
   LOG_ASSERT_ERROR(pthread_join_func != NULL, "Could not find pthread_join");

   int ret;
   PIN_CallApplicationFunction(context, PIN_ThreadId(),
         CALLINGSTD_DEFAULT,
         pthread_join_func,
         PIN_PARG(int), &ret,
         PIN_PARG(pthread_t), thread,
         PIN_PARG(void**), thead_return,
         PIN_PARG_END());
   LOG_ASSERT_ERROR(ret == 0, "pthread_join() returned(%i)", ret);

   // We dont need to delete the thread descriptor
   return ret;
}

IntPtr nullFunction()
{
   LOG_PRINT("In nullFunction()");
   return IntPtr(0);
}
