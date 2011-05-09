#include "event.h"
#include "event_manager.h"
#include "simulator.h"
#include "core_manager.h"
#include "core.h"
#include "network.h"
#include "memory_manager.h"
#include "performance_model.h"

std::map<UInt32,Event::Handler> Event::_handler_map;

Event::Event(Type type, UInt64 time, UnstructuredBuffer* event_args)
   : _type(type), _time(time), _event_args(event_args)
{}

Event::~Event()
{
   if (_event_args)
      delete _event_args;
}

void
Event::processInOrder(Event* event, core_id_t recv_core_id, EventQueue::Type event_queue_type)
{
   LOG_PRINT("Queueing Event (Type[%u], Time[%llu], Processing Core Id[%i])", \
         event->getType(), event->getTime(), recv_core_id);

   Sim()->getEventManager()->processEventInOrder(event, recv_core_id, event_queue_type);
}

void
Event::process()
{
   LOG_PRINT("Processing Event (Type[%u], Time[%llu]) enter", _type, _time);
   
   if (_handler_map[_type])
   {
      _handler_map[_type](this);
   }
   else
   {
      LOG_ASSERT_ERROR(_type >= 0 && _type < Event::NUM_TYPES, "type(%u)", _type);
      __process();
   }
   
   LOG_PRINT("Processing Event (Type[%u], Time[%llu]) exit", _type, _time);
}

void
Event::registerHandler(UInt32 type, Handler handler)
{
   assert(!_handler_map[type]);
   LOG_ASSERT_ERROR(type >= Event::NUM_TYPES, "Event Type(%u) < NUM_TYPES(%u)",
         type, Event::NUM_TYPES);

   LOG_PRINT("registerHandler(Type[%u], Handler[%p])", type, handler);
   _handler_map[type] = handler;
}

void
Event::unregisterHandler(UInt32 type)
{
   assert(_handler_map[type]);
   LOG_ASSERT_ERROR(type >= Event::NUM_TYPES, "Event Type(%u) < NUM_TYPES(%u)",
         type, Event::NUM_TYPES);

   LOG_PRINT("unregisterHandler(Type[%u])", type);
   _handler_map.erase(type);
}

void
EventNetwork::__process()
{
   core_id_t next_hop;
   NetPacket* net_packet;

   (*_event_args) >> next_hop >> net_packet;

   Core* core = Sim()->getCoreManager()->getCoreFromID(next_hop);
   assert(core);
   Network* network = core->getNetwork();
   network->processPacket(net_packet);
}

void
EventInitiateMemoryAccess::__process()
{
   Core* core;
   UInt32 memory_access_id;
   MemComponent::component_t mem_component;
   Core::lock_signal_t lock_signal;
   Core::mem_op_t mem_op_type;
   IntPtr address;
   Byte* data_buffer;
   UInt32 bytes;
   bool modeled;

   (*_event_args) >> core >> memory_access_id >> mem_component >> lock_signal >> mem_op_type
                  >> address >> data_buffer >> bytes >> modeled;
   
   core->initiateMemoryAccess(_time, memory_access_id, mem_component, lock_signal, mem_op_type,
         address, data_buffer, bytes, modeled);
}

void
EventCompleteMemoryAccess::__process()
{
   Core* core;
   UInt32 memory_access_id;

   (*_event_args) >> core >> memory_access_id;

   PerformanceModel* performance_model = core->getPerformanceModel();
   assert(performance_model);

   performance_model->handleCompletedMemoryAccess(_time, memory_access_id);
}

void
EventInitiateCacheAccess::__process()
{
   MemoryManager* memory_manager;
   MemComponent::component_t mem_component;
   UInt32 memory_access_id;
   Core::lock_signal_t lock_signal;
   Core::mem_op_t mem_op_type;
   IntPtr ca_address;
   UInt32 offset;
   Byte* data_buffer;
   UInt32 bytes;
   bool modeled;

   (*_event_args) >> memory_manager >> mem_component >> memory_access_id
                  >> lock_signal >> mem_op_type >> ca_address >> offset
                  >> data_buffer >> bytes >> modeled;

   memory_manager->initiateCacheAccess(_time, mem_component,
         memory_access_id, lock_signal, mem_op_type,
         ca_address, offset, data_buffer, bytes, modeled);
   
}

void
EventReInitiateCacheAccess::__process()
{
   MemoryManager* memory_manager;
   MemComponent::component_t mem_component;
   MissStatus* miss_status;

   (*_event_args) >> memory_manager >> mem_component >> miss_status;

   memory_manager->reInitiateCacheAccess(_time, mem_component, miss_status);
}

void
EventCompleteCacheAccess::__process()
{
   Core* core;
   UInt32 memory_access_id;

   (*_event_args) >> core >> memory_access_id;

   core->completeCacheAccess(_time, memory_access_id);
}

void
EventStartThread::__process()
{
   core_id_t core_id;
   (*_event_args) >> core_id;

   Sim()->getThreadInterface(core_id)->iterate();
}

void
EventResumeThread::__process()
{
   core_id_t core_id;
   (*_event_args) >> core_id;

   Sim()->getThreadInterface(core_id)->iterate();
}
