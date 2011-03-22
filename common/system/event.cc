#include "event.h"
#include "event_manager.h"
#include "simulator.h"
#include "core_manager.h"
#include "core.h"
#include "network.h"
#include "memory_manager.h"
#include "performance_model.h"
#include "cycle_accurate/performance_model.h"

void
EventNetwork::process()
{
   core_id_t next_hop;
   NetPacket* net_packet;

   _event_args >> next_hop >> net_packet;

   // The EventNetwork should have different arguments
   if (Config::getSingleton()->getSimulationMode() == Config::CYCLE_ACCURATE)
   {
      Core* core = Sim()->getCoreManager()->getCoreFromID(next_hop);
      assert(core);
      Network* network = core->getNetwork();
      network->processPacket(net_packet);
   }
   else // (mode == FULL) || (mode == LITE)
   {
      Byte* buffer = net_packet->makeBuffer();
      Transport::Node* transport_node = Transport::getSingleton()->getGlobalNode();
      transport_node->send(next_hop, buffer, net_packet->bufferSize());
      delete [] buffer;
      net_packet->release();
   }
}

void
EventInstruction::process()
{
   CycleAccurate::PerformanceModel* performance_model;

   _event_args >> performance_model;

   performance_model->processNextInstruction();
}

void
EventInitiateMemoryAccess::process()
{
   Core* core;
   MemComponent::component_t mem_component;
   Core::lock_signal_t lock_signal;
   Core::mem_op_t mem_op_type;
   IntPtr address;
   Byte* data_buffer;
   UInt32 bytes;
   bool modeled;

   _event_args >> core >> mem_component >> lock_signal >> mem_op_type
               >> address >> data_buffer >> bytes >> modeled;
   
   core->initiateMemoryAccess(_time, mem_component, lock_signal, mem_op_type,
         address, data_buffer, bytes, modeled);
}

void
EventCompleteMemoryAccess::process()
{
   PerformanceModel* performance_model;
   DynamicInstructionInfo info;

   _event_args >> performance_model >> info;

   if (Config::getSingleton()->getSimulationMode() == Config::CYCLE_ACCURATE)
      ((CycleAccurate::PerformanceModel*) performance_model)->processDynamicInstructionInfo(info);
   else // ((mode == FULL) || (mode == LITE))
      performance_model->pushDynamicInstructionInfo(info);
}

void
EventInitiateCacheAccess::process()
{
   MemoryManager* memory_manager;
   UInt32 memory_access_id;
   MemComponent::component_t mem_component;
   Core::lock_signal_t lock_signal;
   Core::mem_op_t mem_op_type;
   IntPtr ca_address;
   UInt32 offset;
   Byte* data_buffer;
   UInt32 bytes;
   bool modeled;

   _event_args >> memory_manager >> memory_access_id >> mem_component
               >> lock_signal >> mem_op_type >> ca_address >> offset
               >> data_buffer >> bytes >> modeled;

   memory_manager->coreInitiateCacheAccess(_time, memory_access_id,
         mem_component, lock_signal, mem_op_type,
         ca_address, offset, data_buffer, bytes, modeled);
   
}

void
EventCompleteCacheAccess::process()
{
   Core* core;
   UInt32 memory_access_id;

   _event_args >> core >> memory_access_id;

   core->completeCacheAccess(_time, memory_access_id);
}

void
Event::processInOrder(Event* event, core_id_t recv_core_id, EventQueue::Type event_queue_type)
{
   if (Config::getSingleton()->getSimulationMode() == Config::CYCLE_ACCURATE)
   {
      Sim()->getEventManager()->processEventInOrder(event, recv_core_id, event_queue_type);
   }
   else // ((mode == FULL) || (mode == LITE))
   {
      event->process();
      delete event;
   }
}
