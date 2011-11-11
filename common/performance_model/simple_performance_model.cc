#include <iostream>
#include "simulator.h"
#include "thread_interface.h"
#include "simple_performance_model.h"
#include "core.h"
#include "event.h"

SimplePerformanceModel::SimplePerformanceModel(Core* core, float frequency)
   : PerformanceModel(core, frequency)
   , _last_memory_access_id(0)
   , _large_data_buffer(NULL)
{}

SimplePerformanceModel::~SimplePerformanceModel()
{}

void
SimplePerformanceModel::outputSummary(ostream& out)
{
   out << "Core Performance Model Summary:" << endl;
   PerformanceModel::outputSummary(out);
}

bool
SimplePerformanceModel::handleInstruction(Instruction* instruction,
                                          bool atomic_memory_update,
                                          MemoryAccessList* memory_access_list)
{
   LOG_PRINT("handleInstruction(Instruction[%p], atomic_memory_update[%s], memory_access_list[%u])",
         instruction, atomic_memory_update ? "YES" : "NO", memory_access_list->size());

   LOG_ASSERT_ERROR(isEnabled(), "Not Enabled Currently");

   _curr_instruction_status.update(_cycle_count, instruction, atomic_memory_update, memory_access_list);
   
   bool cont = issueNextMemoryRequest();
   return cont;
}

void
SimplePerformanceModel::handleCompletedMemoryAccess(UInt64 time, UInt32 memory_access_id)
{
   _curr_instruction_status._cycle_count = time;
   _curr_instruction_status._curr_memory_operand_num ++;

   // Delete the large data buffer if it has been used
   if (_large_data_buffer)
   {
      delete [] _large_data_buffer;
      _large_data_buffer = NULL;
   }

   // Issue memory request to next address
   issueNextMemoryRequest();
}

bool
SimplePerformanceModel::issueNextMemoryRequest()
{
   UInt32 operand_num = _curr_instruction_status._curr_memory_operand_num;
   if (operand_num == _curr_instruction_status._total_memory_operands)
   {
      completeInstruction();
      return true;
   }
   else
   {
      IntPtr address = (*_curr_instruction_status._memory_access_list)[operand_num].first;
      UInt32 size = (*_curr_instruction_status._memory_access_list)[operand_num].second;

      Core::lock_signal_t lock_signal;
      Core::mem_op_t mem_op_type;
      if (operand_num < _curr_instruction_status._total_read_memory_operands)
      {
         // Read Memory
         lock_signal = (_curr_instruction_status._atomic_memory_update) ? Core::LOCK : Core::NONE;
         mem_op_type = (_curr_instruction_status._atomic_memory_update) ? Core::READ_EX : Core::READ;
      }
      else
      {
         // Write Memory
         lock_signal = (_curr_instruction_status._atomic_memory_update) ? Core::UNLOCK : Core::NONE;
         mem_op_type = Core::WRITE;
      }

      Byte* data_buffer;
      assert(size > 0);
      if (size <= SCRATCHPAD_SIZE)
      {
         data_buffer = _data_buffer;
      }
      else // (size > SCRATCHPAD_SIZE)
      {
         assert(!_large_data_buffer);
         _large_data_buffer = new Byte[size];
         data_buffer = _large_data_buffer;
      }

      UnstructuredBuffer* event_args = new UnstructuredBuffer();
      (*event_args) << getCore()
                    << _last_memory_access_id ++
                    << MemComponent::L1_DCACHE << lock_signal << mem_op_type
                    << address << data_buffer << size
                    << true /* modeled */;
      LOG_PRINT("Event args size(%u)", event_args->size());
      EventInitiateMemoryAccess* event = new EventInitiateMemoryAccess(_curr_instruction_status._cycle_count,
                                                                       event_args);
      Event::processInOrder(event, getCore()->getId(), EventQueue::ORDERED);

      return false;
   }
}

void
SimplePerformanceModel::completeInstruction()
{
   _curr_instruction_status._cycle_count += _curr_instruction_status._instruction->getCost();
   _cycle_count = _curr_instruction_status._cycle_count;

   _curr_instruction_status._instruction = (Instruction*) NULL;
   delete _curr_instruction_status._memory_access_list;

   // Update Performance Counters
   _total_instructions_executed ++;
   if ((_total_instructions_executed % _max_outstanding_instructions) == 0)
   {
      Sim()->getThreadInterface(getCore()->getId())->sendSimInsReply(_max_outstanding_instructions);
   }

   UnstructuredBuffer* event_args = new UnstructuredBuffer();
   (*event_args) << getCore()->getId();
   EventResumeThread* event = new EventResumeThread(_cycle_count, event_args);
   Event::processInOrder(event, getCore()->getId(), EventQueue::ORDERED);
}

SimplePerformanceModel::InstructionStatus::InstructionStatus()
   : _cycle_count(0)
   , _instruction(NULL)
   , _atomic_memory_update(false)
   , _memory_access_list(NULL)
   , _curr_memory_operand_num(0)
   , _total_read_memory_operands(0)
   , _total_write_memory_operands(0)
   , _total_memory_operands(0)
{}

SimplePerformanceModel::InstructionStatus::~InstructionStatus()
{}

void
SimplePerformanceModel::InstructionStatus::update(UInt64 cycle_count,
                                                  Instruction* instruction,
                                                  bool atomic_memory_update,
                                                  MemoryAccessList* memory_access_list)
{
   _cycle_count = cycle_count;
   _instruction = instruction;
   _atomic_memory_update = atomic_memory_update;
   _memory_access_list = memory_access_list;
   _curr_memory_operand_num = 0;
   _total_read_memory_operands = instruction->getNumOperands(Operand::MEMORY, Operand::READ);
   _total_write_memory_operands = instruction->getNumOperands(Operand::MEMORY, Operand::WRITE);
   _total_memory_operands = memory_access_list->size();
   assert(_total_memory_operands == (_total_read_memory_operands + _total_write_memory_operands));
}
