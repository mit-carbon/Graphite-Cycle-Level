#include "routine_manager.h"
#include "simulator.h"
#include "core_manager.h"
#include "core.h"
#include "thread_interface.h"
#include "app_request.h"
#include "packetize.h"

#include "carbon_user.h"

IntPtr
emulateRoutine(Routine::Id routine_id)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << routine_id;
   return emulateRoutine(routine_info);
}

IntPtr
emulateRoutine(UnstructuredBuffer* routine_info)
{
   Core* core = Sim()->getCoreManager()->getCurrentCore();
   LOG_PRINT("Core ID(%i): EmulateRoutine()", core->getId());
   // Send App Request to Sim Thread
   AppRequest app_request(AppRequest::EMULATE_ROUTINE, routine_info);
   Sim()->getThreadInterface(core->getId())->sendAppRequest(app_request);

   // Receive Reply from Sim Thread
   SimReply sim_reply = Sim()->getThreadInterface(core->getId())->recvSimReply();
   return sim_reply;
}

void
__emulateRoutine(UInt64 time, core_id_t core_id, Routine::Id routine_id, UnstructuredBuffer* routine_args)
{
   bool cont = false;
   IntPtr ret_val = 0;

   LOG_PRINT("__emulateRoutine(%llu,%i,%u,%p)", time, core_id, routine_id, routine_args);

   switch (routine_id)
   {
   case Routine::CARBON_SPAWN_THREAD:
      {
         thread_func_t func;
         void* arg;
         (*routine_args) >> func >> arg;

         __CarbonSpawnThread(time, core_id, func, arg);
         break;
      }

   case Routine::CARBON_JOIN_THREAD:
      {
         core_id_t join_core_id;
         (*routine_args) >> join_core_id;

         __CarbonJoinThread(time, core_id, join_core_id);
         break;
      }

   case Routine::CARBON_MUTEX_INIT:
      {
         carbon_mutex_t* mux;
         (*routine_args) >> mux;

         __CarbonMutexInit(time, core_id, mux);
         break;
      }

   case Routine::CARBON_MUTEX_LOCK:
      {
         carbon_mutex_t* mux;
         (*routine_args) >> mux;

         __CarbonMutexLock(time, core_id, mux);
         break;
      }

   case Routine::CARBON_MUTEX_UNLOCK:
      {
         carbon_mutex_t* mux;
         (*routine_args) >> mux;

         __CarbonMutexUnlock(time, core_id, mux);
         break;
      }

   case Routine::CARBON_COND_INIT:
      {
         carbon_cond_t* cond;
         (*routine_args) >> cond;

         __CarbonCondInit(time, core_id, cond);
         break;
      }

   case Routine::CARBON_COND_WAIT:
      {
         carbon_cond_t* cond;
         carbon_mutex_t* mux;
         (*routine_args) >> cond >> mux;

         __CarbonCondWait(time, core_id, cond, mux);
         break;
      }

   case Routine::CARBON_COND_SIGNAL:
      {
         carbon_cond_t* cond;
         (*routine_args) >> cond;

         __CarbonCondSignal(time, core_id, cond);
         break;
      }

   case Routine::CARBON_COND_BROADCAST:
      {
         carbon_cond_t* cond;
         (*routine_args) >> cond;

         __CarbonCondBroadcast(time, core_id, cond);
         break;
      }

   case Routine::CARBON_BARRIER_INIT:
      {
         carbon_barrier_t* barrier;
         UInt32 count;
         (*routine_args) >> barrier >> count;

         __CarbonBarrierInit(time, core_id, barrier, count);
         break;
      }

   case Routine::CARBON_BARRIER_WAIT:
      {
         carbon_barrier_t* barrier;
         (*routine_args) >> barrier;

         __CarbonBarrierWait(time, core_id, barrier);
         break;
      }

   case Routine::CAPI_INITIALIZE:
      {
         int rank;
         (*routine_args) >> rank;
      
         ret_val = (IntPtr) __CAPI_Initialize(core_id, rank);
         cont = true;
         break;
      }

   case Routine::CAPI_RANK:
      {
         int* rank_ptr;
         (*routine_args) >> rank_ptr;
      
         ret_val = (IntPtr) __CAPI_rank(core_id, rank_ptr);
         cont = true;
         break;
      }

   case Routine::CAPI_MESSAGE_SEND:
      {
         CAPI_endpoint_t sender;
         CAPI_endpoint_t receiver;
         char* buffer;
         int size;
         (*routine_args) >> sender >> receiver >> buffer >> size;

         __CAPI_message_send_w(core_id, sender, receiver, buffer, size);
         break;
      }

   case Routine::CAPI_MESSAGE_SEND_EXPLICIT:
      {
         CAPI_endpoint_t sender;
         CAPI_endpoint_t receiver;
         char* buffer;
         int size;
         carbon_network_t carbon_net_type;
         (*routine_args) >> sender >> receiver >> buffer >> size >> carbon_net_type;

         __CAPI_message_send_w_ex(core_id, sender, receiver, buffer, size, carbon_net_type);
         break;
      }

   case Routine::CAPI_MESSAGE_RECEIVE:
      {
         CAPI_endpoint_t sender;
         CAPI_endpoint_t receiver;
         char* buffer;
         int size;
         (*routine_args) >> sender >> receiver >> buffer >> size;

         __CAPI_message_receive_w(core_id, sender, receiver, buffer, size);
         break;
      }

   case Routine::CAPI_MESSAGE_RECEIVE_EXPLICIT:
      {
         CAPI_endpoint_t sender;
         CAPI_endpoint_t receiver;
         char* buffer;
         int size;
         carbon_network_t carbon_net_type;
         (*routine_args) >> sender >> receiver >> buffer >> size >> carbon_net_type;

         __CAPI_message_receive_w_ex(core_id, sender, receiver, buffer, size, carbon_net_type);
         break;
      }

   case Routine::CARBON_GET_TIME:
      {
         ret_val = (IntPtr) __CarbonGetTime(core_id);
         cont = true;
         break;
      }

   case Routine::CARBON_GET_CORE_FREQUENCY:
      {
         float* frequency;
         (*routine_args) >> frequency;

         __CarbonGetCoreFrequency(core_id, frequency);
         cont = true;
         break;
      }

   case Routine::CARBON_SET_CORE_FREQUENCY:
      {
         float* frequency;
         (*routine_args) >> frequency;

         __CarbonSetCoreFrequency(core_id, frequency);
         cont = true;
         break;
      }

   case Routine::ENABLE_PERFORMANCE_MODELS:
      {
         Simulator::__enablePerformanceModels();
         cont = true;
         break;
      }

   case Routine::DISABLE_PERFORMANCE_MODELS:
      {
         Simulator::__disablePerformanceModels();
         cont = true;
         break;
      }

   default:
      LOG_PRINT_ERROR("Unrecongized Routine Id(%u)", routine_id);
      break;
   }

   if (cont)
   {
      Sim()->getThreadInterface(core_id)->sendSimReply(time, ret_val);
   }
}
