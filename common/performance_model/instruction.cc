#include "instruction.h"
#include "simulator.h"
#include "core_manager.h"
#include "core.h"
#include "performance_model.h"
#include "branch_predictor.h"

// Instruction

Instruction::StaticInstructionCosts Instruction::m_instruction_costs;

Instruction::Instruction(InstructionType type, OperandList &operands)
   : m_type(type)
   , m_addr(0)
   , m_operands(operands)
{
}

Instruction::Instruction(InstructionType type)
   : m_type(type)
   , m_addr(0)
{
}

InstructionType Instruction::getType()
{
    return m_type;
}

UInt64 Instruction::getCost()
{
   LOG_ASSERT_ERROR(m_type < MAX_INSTRUCTION_COUNT, "Unknown instruction type: %d", m_type);
   return Instruction::m_instruction_costs[m_type]; 
}

void Instruction::initializeStaticInstructionModel()
{
   m_instruction_costs.resize(MAX_INSTRUCTION_COUNT);
   for(unsigned int i = 0; i < MAX_INSTRUCTION_COUNT; i++)
   {
       char key_name [1024];
       snprintf(key_name, 1024, "perf_model/core/static_instruction_costs/%s", INSTRUCTION_NAMES[i]);
       UInt32 instruction_cost = Sim()->getCfg()->getInt(key_name, 0);
       m_instruction_costs[i] = instruction_cost;
   }
}

UInt32 Instruction::getNumOperands(Operand::Type operand_type, Operand::Direction operand_direction)
{
   UInt32 number = 0;
   OperandList::iterator it = m_operands.begin();
   for ( ; it != m_operands.end(); it ++)
   {
      if ( ((*it).m_type == operand_type) && ((*it).m_direction == operand_direction) )
         number ++;
   }
   return number;
}
