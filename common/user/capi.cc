#include "simulator.h"
#include "core_manager.h"
#include "routine_manager.h"
#include "config.h"
#include "core.h"
#include "carbon_user.h"
#include "log.h"

CAPI_return_t CAPI_rank(int* rank)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CAPI_RANK << rank;
   return (CAPI_return_t) emulateRoutine(routine_info);
}

CAPI_return_t __CAPI_rank(core_id_t core_id, int* rank)
{
   *rank = core_id;
   return 0;
}

CAPI_return_t CAPI_Initialize(int rank)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CAPI_INITIALIZE << rank;
   return (CAPI_return_t) emulateRoutine(routine_info);
}

CAPI_return_t __CAPI_Initialize(core_id_t core_id, int rank)
{
   LOG_PRINT("Initializing comm_id: %d to core_id: %d", rank, core_id);
   Config::getSingleton()->updateCommToCoreMap(rank, core_id);
   return 0;
}

CAPI_return_t CAPI_message_send_w(CAPI_endpoint_t sender,
                                  CAPI_endpoint_t receiver,
                                  char* buffer,
                                  int size)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CAPI_MESSAGE_SEND << sender << receiver << buffer << size;
   return (CAPI_return_t) emulateRoutine(routine_info);
}

void __CAPI_message_send_w(core_id_t core_id,
                           CAPI_endpoint_t sender, 
                           CAPI_endpoint_t receiver, 
                           char *buffer, 
                           int size)
{

   LOG_PRINT("SimSendW - sender: %d, recv: %d, size: %d", sender, receiver, size);

   core_id_t sending_core = Config::getSingleton()->getCoreFromCommId(sender);
   
   core_id_t receiving_core = CAPI_ENDPOINT_ALL;
   if (receiver != CAPI_ENDPOINT_ALL)
      receiving_core = Config::getSingleton()->getCoreFromCommId(receiver);

   Core* core = Sim()->getCoreManager()->getCoreFromID(core_id);
   core->sendMsg(sending_core, receiving_core, buffer, size, (carbon_network_t) CARBON_NET_USER_1);
}

CAPI_return_t CAPI_message_send_w_ex(CAPI_endpoint_t sender,
                                     CAPI_endpoint_t receiver,
                                     char* buffer,
                                     int size,
                                     carbon_network_t net_type)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CAPI_MESSAGE_SEND_EXPLICIT << sender << receiver << buffer << size << net_type;
   return (CAPI_return_t) emulateRoutine(routine_info);
}

void __CAPI_message_send_w_ex(core_id_t core_id,
                              CAPI_endpoint_t sender, 
                              CAPI_endpoint_t receiver, 
                              char *buffer,
                              int size,
                              carbon_network_t net_type)
{
   LOG_PRINT("SimSendW - sender: %d, recv: %d, size: %d", sender, receiver, size);

   core_id_t sending_core = Config::getSingleton()->getCoreFromCommId(sender);
   
   core_id_t receiving_core = CAPI_ENDPOINT_ALL;
   if (receiver != CAPI_ENDPOINT_ALL)
      receiving_core = Config::getSingleton()->getCoreFromCommId(receiver);

   Core* core = Sim()->getCoreManager()->getCoreFromID(core_id);
   core->sendMsg(sending_core, receiving_core, buffer, size, net_type);
}

CAPI_return_t CAPI_message_receive_w(CAPI_endpoint_t sender,
                                     CAPI_endpoint_t receiver,
                                     char* buffer,
                                     int size)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CAPI_MESSAGE_RECEIVE << sender << receiver << buffer << size;
   return (CAPI_return_t) emulateRoutine(routine_info);
}

void __CAPI_message_receive_w(core_id_t core_id,
                              CAPI_endpoint_t sender, 
                              CAPI_endpoint_t receiver,
                              char *buffer,
                              int size)
{
   LOG_PRINT("SimRecvW - sender: %d, recv: %d, size: %d", sender, receiver, size);

   core_id_t sending_core = CAPI_ENDPOINT_ANY;
   if (sender != CAPI_ENDPOINT_ANY)
      sending_core = Config::getSingleton()->getCoreFromCommId(sender);
   
   core_id_t receiving_core = Config::getSingleton()->getCoreFromCommId(receiver);

   Core* core = Sim()->getCoreManager()->getCoreFromID(core_id);
   core->recvMsg(sending_core, receiving_core, buffer, size, (carbon_network_t) CARBON_NET_USER_1);
}

CAPI_return_t CAPI_message_receive_w_ex(CAPI_endpoint_t sender,
                                        CAPI_endpoint_t receiver,
                                        char* buffer,
                                        int size,
                                        carbon_network_t net_type)
{
   UnstructuredBuffer* routine_info = new UnstructuredBuffer();
   (*routine_info) << Routine::CAPI_MESSAGE_RECEIVE_EXPLICIT << sender << receiver << buffer << size << net_type;
   return (CAPI_return_t) emulateRoutine(routine_info);
}

void __CAPI_message_receive_w_ex(core_id_t core_id,
                                 CAPI_endpoint_t sender,
                                 CAPI_endpoint_t receiver,
                                 char *buffer,
                                 int size,
                                 carbon_network_t net_type)
{
   LOG_PRINT("SimRecvW - sender: %d, recv: %d, size: %d", sender, receiver, size);

   core_id_t sending_core = CAPI_ENDPOINT_ANY;
   if (sender != CAPI_ENDPOINT_ANY)
      sending_core = Config::getSingleton()->getCoreFromCommId(sender);
   
   core_id_t receiving_core = Config::getSingleton()->getCoreFromCommId(receiver);

   Core* core = Sim()->getCoreManager()->getCoreFromID(core_id);
   core->recvMsg(sending_core, receiving_core, buffer, size, net_type);
}
