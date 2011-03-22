#include "performance_model.h"
#include "cycle_accurate/performance_model.h"
#include "simple_performance_model.h"
#include "iocoom_performance_model.h"
#include "magic_performance_model.h"
#include "simulator.h"
#include "core_manager.h"
#include "config.h"
#include "core.h"
#include "branch_predictor.h"
#include "fxsupport.h"
#include "utils.h"

PerformanceModel* PerformanceModel::create(Core* core)
{
   // Create another core model for the cycle-accurate stuff
   if (Config::getSingleton()->getSimulationMode() == Config::CYCLE_ACCURATE)
      return CycleAccurate::PerformanceModel::create(core);

   volatile float frequency = Config::getSingleton()->getCoreFrequency(core->getId());
   string core_model = Config::getSingleton()->getCoreType(core->getId());

   if (core_model == "iocoom")
      return new IOCOOMPerformanceModel(core, frequency);
   else if (core_model == "simple")
      return new SimplePerformanceModel(core, frequency);
   else if (core_model == "magic")
      return new MagicPerformanceModel(core, frequency);
   else
   {
      LOG_PRINT_ERROR("Invalid perf model type: %s", core_model.c_str());
      return NULL;
   }
}

// Public Interface
PerformanceModel::PerformanceModel(Core *core, float frequency)
   : m_cycle_count(0)
   , m_core(core)
   , m_frequency(frequency)
   , m_average_frequency(0.0)
   , m_total_time(0)
   , m_checkpointed_cycle_count(0)
   , m_enabled(false)
   , m_current_ins_index(0)
   , m_bp(0)
{
   // Create Branch Predictor
   m_bp = BranchPredictor::create();

   // Initialize Instruction Counters
   initializeInstructionCounters();
}

PerformanceModel::~PerformanceModel()
{
   delete m_bp; m_bp = 0;
}

void PerformanceModel::outputSummary(ostream& os)
{
   // Frequency Summary
   frequencySummary(os);
   
   // Instruction Counter Summary
   os << "    Recv Instructions: " << m_total_recv_instructions << endl;
   os << "    Recv Instruction Costs: " << m_total_recv_instruction_costs << endl;
   os << "    Sync Instructions: " << m_total_sync_instructions << endl;
   os << "    Sync Instruction Costs: " << m_total_sync_instruction_costs << endl;

   // Branch Predictor Summary
   if (m_bp)
      m_bp->outputSummary(os);
}

void PerformanceModel::frequencySummary(ostream& os)
{
   os << "    Completion Time: " \
      << (UInt64) (((float) m_cycle_count) / m_frequency) \
      << endl;
   os << "    Average Frequency: " << m_average_frequency << endl;
}

void PerformanceModel::enable()
{
   // MCP perf model should never be enabled
   if (m_core->getId() == Config::getSingleton()->getMCPCoreNum())
      return;

   m_enabled = true;
}

void PerformanceModel::disable()
{
   m_enabled = false;
}

// This function is called:
// 1) Whenever frequency is changed
void PerformanceModel::updateInternalVariablesOnFrequencyChange(volatile float frequency)
{
   recomputeAverageFrequency();
   
   volatile float old_frequency = m_frequency;
   volatile float new_frequency = frequency;
   
   m_checkpointed_cycle_count = (UInt64) (((double) m_cycle_count / old_frequency) * new_frequency);
   m_cycle_count = m_checkpointed_cycle_count;
   m_frequency = new_frequency;
}

// This function is called:
// 1) On thread start
void PerformanceModel::setCycleCount(UInt64 cycle_count)
{
   m_checkpointed_cycle_count = cycle_count;
   m_cycle_count = cycle_count;
}

// This function is called:
// 1) On thread exit
// 2) Whenever frequency is changed
void PerformanceModel::recomputeAverageFrequency()
{
   volatile double cycles_elapsed = (double) (m_cycle_count - m_checkpointed_cycle_count);
   volatile double total_cycles_executed = (m_average_frequency * m_total_time) + cycles_elapsed;
   volatile double total_time_taken = m_total_time + (cycles_elapsed / m_frequency);

   m_average_frequency = total_cycles_executed / total_time_taken;
   m_total_time = (UInt64) total_time_taken;
}

void PerformanceModel::initializeInstructionCounters()
{
   m_total_recv_instructions = 0;
   m_total_recv_instruction_costs = 0;
   m_total_sync_instructions = 0;
   m_total_sync_instruction_costs = 0;
}

void PerformanceModel::updateInstructionCounters(Instruction* i)
{
   switch (i->getType())
   {
      case INST_RECV:
         m_total_recv_instructions ++;
         m_total_recv_instruction_costs += i->getCost();
         break;

      default:
         break;
   }
}

void PerformanceModel::queueDynamicInstruction(Instruction *i)
{
   if (!m_enabled || !Config::getSingleton()->getEnablePerformanceModeling())
   {
      delete i;
      return;
   }

   // Update Instruction Counters
   updateInstructionCounters(i);

   BasicBlock *bb = new BasicBlock(true);
   bb->push_back(i);
   ScopedLock sl(m_basic_block_queue_lock);
   m_basic_block_queue.push(bb);
}

void PerformanceModel::queueBasicBlock(BasicBlock *basic_block)
{
   if (!m_enabled || !Config::getSingleton()->getEnablePerformanceModeling())
      return;

   ScopedLock sl(m_basic_block_queue_lock);
   m_basic_block_queue.push(basic_block);
}

//FIXME: this will go in a thread
void PerformanceModel::iterate()
{
   // Because we will sometimes not have info available (we will throw
   // a DynamicInstructionInfoNotAvailable), we need to be able to
   // continue from the middle of a basic block. m_current_ins_index
   // tracks which instruction we are currently on within the basic
   // block.

   ScopedLock sl(m_basic_block_queue_lock);

   while (m_basic_block_queue.size() > 1)
   {
      BasicBlock *current_bb = m_basic_block_queue.front();

      try
      {
         for( ; m_current_ins_index < current_bb->size(); m_current_ins_index++)
         {
            try
            {
               handleInstruction(current_bb->at(m_current_ins_index));
            }
            catch (AbortInstructionException)
            {
               // move on to next ...
            }
         }

         if (current_bb->isDynamic())
            delete current_bb;

         m_basic_block_queue.pop();
         m_current_ins_index = 0; // move to beginning of next bb
      }
      catch (DynamicInstructionInfoNotAvailableException)
      {
         return;
      }
   }
}

void PerformanceModel::pushDynamicInstructionInfo(DynamicInstructionInfo &i)
{
   if (!m_enabled || !Config::getSingleton()->getEnablePerformanceModeling())
      return;

   ScopedLock sl(m_dynamic_info_queue_lock);
   m_dynamic_info_queue.push(i);
}

void PerformanceModel::popDynamicInstructionInfo()
{
   if (!m_enabled || !Config::getSingleton()->getEnablePerformanceModeling())
      return;

   ScopedLock sl(m_dynamic_info_queue_lock);
   LOG_ASSERT_ERROR(m_dynamic_info_queue.size() > 0,
                    "Expected some dynamic info to be available.");
   LOG_ASSERT_ERROR(m_dynamic_info_queue.size() < 5000,
                    "Dynamic info queue is growing too big.");
   m_dynamic_info_queue.pop();
}

DynamicInstructionInfo& PerformanceModel::getDynamicInstructionInfo()
{
   ScopedLock sl(m_dynamic_info_queue_lock);

   // Information is needed to model the instruction, but isn't
   // available. This is handled in iterate() by returning early and
   // continuing from that instruction later.

   // FIXME: Note this assumes that either none of the info for an
   // instruction is available or all of it! This works for
   // performance modeling in the same thread as functional modeling,
   // but it WILL NOT work if performance modeling is moved to a
   // separate thread!

   if (m_dynamic_info_queue.empty())
      throw DynamicInstructionInfoNotAvailableException();

   LOG_ASSERT_ERROR(m_dynamic_info_queue.size() < 5000,
                    "Dynamic info queue is growing too big.");

   return m_dynamic_info_queue.front();
}
