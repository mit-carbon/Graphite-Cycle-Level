#include "cycle_accurate/simple_performance_model.h"
#include "core.h"
#include "event.h"

namespace CycleAccurate
{

SimplePerformanceModel::SimplePerformanceModel(Core* core, float frequency)
   : PerformanceModel(core, frequency)
   , _curr_instruction_status(NULL)
   , _waiting(true)
{}

SimplePerformanceModel::~SimplePerformanceModel()
{
   assert(_instruction_status_queue.empty());
}

void
SimplePerformanceModel::queueInstruction(Instruction* instruction,
                                         bool atomic_memory_update,
                                         MemoryAccessList* memory_access_list)
{
   InstructionStatus* instruction_status = new InstructionStatus(instruction, 
                                           atomic_memory_update, memory_access_list);
   ScopedLock sl(_instruction_status_queue_lock);
   _instruction_status_queue.push(instruction_status);
   if (_waiting)
   {
      LOG_PRINT_ERROR("Reached Here - Cycle Accurate Simple Performance Model");

      UnstructuredBuffer event_args;
      event_args << this;
      EventInstruction* event = new EventInstruction(m_cycle_count, event_args);
      Event::processInOrder(event, getCore()->getId(), EventQueue::ORDERED);

      _waiting = false;
   }
}

void
SimplePerformanceModel::processDynamicInstructionInfo(DynamicInstructionInfo& info)
{
   assert((info.type == DynamicInstructionInfo::MEMORY_READ) || (info.type == DynamicInstructionInfo::MEMORY_WRITE));
   MemoryAccessList* memory_access_list = _curr_instruction_status->_memory_access_list;
   UInt32 operand_num = _curr_instruction_status->_curr_memory_operand_num;
   assert( ((*memory_access_list)[operand_num]).first == info.memory_info.addr );

   _curr_instruction_status->_time += info.memory_info.latency;
   _curr_instruction_status->_curr_memory_operand_num ++;

   // Issue memory request to next address
   issueNextMemoryRequest();
}

void
SimplePerformanceModel::processNextInstruction()
{
   // Get Next Instruction
   bool present = getNextInstruction();

   if (present)
   {
      // Process Next Instruction
      processInstruction();
   }
   else
   {
      // Wait for the Next Instruction
      _waiting = true;
   }
}

bool
SimplePerformanceModel::getNextInstruction()
{
   ScopedLock sl(_instruction_status_queue_lock); 
  
   _instruction_status_queue.pop();
   _curr_instruction_status = (!_instruction_status_queue.empty()) ? 
                              _instruction_status_queue.front() : (InstructionStatus*) NULL;
   return (!_instruction_status_queue.empty());
}

void
SimplePerformanceModel::processInstruction()
{
   _curr_instruction_status->_time = getCycleCount();
   issueNextMemoryRequest();
}

void
SimplePerformanceModel::issueNextMemoryRequest()
{
   UInt32 operand_num = _curr_instruction_status->_curr_memory_operand_num;
   if (operand_num == _curr_instruction_status->_total_memory_operands)
   {
      completeInstruction();
   }
   else
   {
      IntPtr address = (*_curr_instruction_status->_memory_access_list)[operand_num].first;
      UInt32 size = (*_curr_instruction_status->_memory_access_list)[operand_num].second;

      Core::lock_signal_t lock_signal;
      Core::mem_op_t mem_op_type;
      if (operand_num < _curr_instruction_status->_total_read_memory_operands)
      {
         // Read Memory
         lock_signal = (_curr_instruction_status->_atomic_memory_update) ? Core::LOCK : Core::NONE;
         mem_op_type = (_curr_instruction_status->_atomic_memory_update) ? Core::READ_EX : Core::READ;
      }
      else
      {
         // Write Memory
         lock_signal = (_curr_instruction_status->_atomic_memory_update) ? Core::UNLOCK : Core::NONE;
         mem_op_type = Core::WRITE;
      }

      Byte data_buffer[size];

      UnstructuredBuffer event_args;
      event_args << getCore()
                 << MemComponent::L1_DCACHE << lock_signal << mem_op_type
                 << address << ((Byte*) data_buffer) << size
                 << true /* modeled */;
      EventInitiateMemoryAccess* event = new EventInitiateMemoryAccess(_curr_instruction_status->_time,
                                                                       event_args);
      Event::processInOrder(event, getCore()->getId(), EventQueue::ORDERED);
   }
}

void
SimplePerformanceModel::completeInstruction()
{
   _curr_instruction_status->_time += _curr_instruction_status->_instruction->getCost();
   m_cycle_count = _curr_instruction_status->_time;

   delete _curr_instruction_status;
   processNextInstruction();
}

SimplePerformanceModel::InstructionStatus::InstructionStatus(Instruction* instruction,
      bool atomic_memory_update, MemoryAccessList* memory_access_list)
   : _instruction(instruction)
   , _atomic_memory_update(atomic_memory_update)
   , _memory_access_list(memory_access_list)
   , _time(0) // Assign this when I start processing the instruction
   , _curr_memory_operand_num(0)
{
   _total_read_memory_operands = instruction->getNumOperands(Operand::MEMORY, Operand::READ);
   _total_write_memory_operands = instruction->getNumOperands(Operand::MEMORY, Operand::WRITE);
   _total_memory_operands = memory_access_list->size();
   assert(_total_memory_operands == (_total_read_memory_operands + _total_write_memory_operands));
}

SimplePerformanceModel::InstructionStatus::~InstructionStatus()
{
   // Created at runtime
   delete _memory_access_list;
}

}
