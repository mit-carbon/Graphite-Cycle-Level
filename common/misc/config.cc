#include "config.h"

#include "network_model.h"
#include "network_types.h"
#include "packet_type.h"
#include "simulator.h"
#include "utils.h"

#include <sstream>
#include "log.h"

#define DEBUG

UInt32 Config::m_knob_total_cores;
UInt32 Config::m_knob_total_sim_threads;
bool Config::m_knob_simarch_has_shared_mem;
std::string Config::m_knob_output_file;
bool Config::m_knob_enable_performance_modeling;
bool Config::m_knob_enable_power_modeling;

using namespace std;

Config *Config::m_singleton;

Config *Config::getSingleton()
{
   assert(m_singleton != NULL);
   return m_singleton;
}

Config::Config()
{
   // NOTE: We can NOT use logging in the config constructor! The log
   // has not been instantiated at this point!
   try
   {
      m_knob_total_cores = Sim()->getCfg()->getInt("general/total_cores");
      m_knob_total_sim_threads = Sim()->getCfg()->getInt("general/num_sim_threads");
      m_knob_simarch_has_shared_mem = Sim()->getCfg()->getBool("general/enable_shared_mem");
      m_knob_output_file = Sim()->getCfg()->getString("general/output_file");
      m_knob_enable_performance_modeling = Sim()->getCfg()->getBool("general/enable_performance_modeling");
      m_knob_enable_power_modeling = Sim()->getCfg()->getBool("general/enable_power_modeling");

      // Accuracy & Execution Modes
      m_accuracy_mode = parseAccuracyMode(Sim()->getCfg()->getString("general/accuracy_mode"));
      m_execution_mode = parseExecutionMode(Sim()->getCfg()->getString("general/execution_mode"));
   }
   catch(...)
   {
      fprintf(stderr, "ERROR: Config obtained a bad value from config.\n");
      exit(EXIT_FAILURE);
   }

   m_total_cores = m_knob_total_cores;
   m_total_sim_threads = m_knob_total_sim_threads;

   m_singleton = this;

   assert(m_total_cores > 0);

   // Parse Network Models - Need to be done here to initialize the network models 
   parseNetworkParameters();

   // Adjust the number of cores corresponding to the network model we use
   m_total_cores = getNearestAcceptableCoreCount(m_total_cores);

   // Parse Core Models
   parseCoreParameters();

   // Length of a core identifier
   m_core_id_length = computeCoreIDLength(m_total_cores);

   // Assert Conditions
   if (m_accuracy_mode != CYCLE_LEVEL)
   {
      fprintf(stderr, "ERROR: Only cycle_level mode allowed\n");
      exit(EXIT_FAILURE);
   }
   if (m_total_sim_threads > m_total_cores)
   {
      fprintf(stderr, "Num Sim Threads (%u) > Num Cores (%u)\n", m_total_sim_threads, m_total_cores); 
      exit(EXIT_FAILURE);
   }
}

Config::~Config()
{}

UInt32 Config::computeCoreIDLength(UInt32 core_count)
{
   UInt32 num_bits = ceilLog2(core_count);
   if ((num_bits % 8) == 0)
      return (num_bits / 8);
   else
      return (num_bits / 8) + 1;
}

// Parse XML config file and use it to fill in config state.  Only modifies
// fields specified in the config file.  Therefore, this method can be used
// to override only the specific options given in the file.
void Config::loadFromFile(char* filename)
{
   return;
}

// Fill in config state from command-line arguments.  Only modifies fields
// specified on the command line.  Therefore, this method can be used to
// override only the specific options given.
void Config::loadFromCmdLine()
{
   return;
}

bool Config::isSimulatingSharedMemory() const
{
   return (bool)m_knob_simarch_has_shared_mem;
}

bool Config::getEnablePerformanceModeling() const
{
   return (bool)m_knob_enable_performance_modeling;
}

bool Config::getEnablePowerModeling() const
{
   return (bool)m_knob_enable_power_modeling;
}

std::string Config::getOutputFileName() const
{
   return formatOutputFileName(m_knob_output_file);
}

std::string Config::formatOutputFileName(string filename) const
{
   assert(Simulator::getConfigFile());
   return (Simulator::getConfigFile()->getString("general/output_dir",".") + "/" + filename);
}

void Config::updateCommToCoreMap(UInt32 comm_id, core_id_t core_id)
{
   m_comm_to_core_map[comm_id] = core_id;
}

UInt32 Config::getCoreFromCommId(UInt32 comm_id)
{
   CommToCoreMap::iterator it = m_comm_to_core_map.find(comm_id);
   return it == m_comm_to_core_map.end() ? INVALID_CORE_ID : it->second;
}

Config::AccuracyMode Config::parseAccuracyMode(string mode)
{
   if (mode == "normal")
      return NORMAL;
   else if (mode == "cycle_level")
      return CYCLE_LEVEL;
   else
   {
      fprintf(stderr, "Unrecognized Accuracy Mode(%s)\n", mode.c_str());
      exit(EXIT_FAILURE);
   }
}

Config::ExecutionMode Config::parseExecutionMode(string mode)
{
   if (mode == "full")
      return FULL;
   else if (mode == "lite")
      return LITE;
   else if (mode == "native")
      return NATIVE;
   else
   {
      fprintf(stderr, "Unrecognized Execution Mode(%s)\n", mode.c_str());
      exit(EXIT_FAILURE);
   }
}

void Config::parseCoreParameters()
{
   // Default values are as follows:
   // 1) Number of cores -> Number of application cores
   // 2) Frequency -> 1 GHz
   // 3) Core Type -> simple

   const UInt32 DEFAULT_NUM_CORES = getTotalCores();
   const float DEFAULT_FREQUENCY = 1;
   const string DEFAULT_CORE_TYPE = "magic";
   const string DEFAULT_CACHE_TYPE = "T1";

   string core_parameter_tuple_str;
   vector<string> core_parameter_tuple_vec;
   try
   {
      core_parameter_tuple_str = Sim()->getCfg()->getString("perf_model/core/model_list");
   }
   catch(...)
   {
      fprintf(stderr, "Could not read perf_model/core/model_list from the cfg file\n");
      exit(EXIT_FAILURE);
   }

   UInt32 num_initialized_cores = 0;

   parseList(core_parameter_tuple_str, core_parameter_tuple_vec, "<>");
   
   for (vector<string>::iterator tuple_it = core_parameter_tuple_vec.begin(); \
         tuple_it != core_parameter_tuple_vec.end(); tuple_it++)
   {
      // Initializing using default values
      UInt32 num_cores = DEFAULT_NUM_CORES;
      float frequency = DEFAULT_FREQUENCY;
      string core_type = DEFAULT_CORE_TYPE;
      string l1_icache_type = DEFAULT_CACHE_TYPE;
      string l1_dcache_type = DEFAULT_CACHE_TYPE;
      string l2_cache_type = DEFAULT_CACHE_TYPE;

      vector<string> core_parameter_tuple;
      parseList(*tuple_it, core_parameter_tuple, ",");
     
      SInt32 param_num = 0; 
      for (vector<string>::iterator param_it = core_parameter_tuple.begin(); \
            param_it != core_parameter_tuple.end(); param_it ++)
      {
         if (*param_it != "default")
         {
            switch (param_num)
            {
               case 0:
                  convertFromString<UInt32>(num_cores, *param_it);
                  break;

               case 1:
                  convertFromString<float>(frequency, *param_it);
                  break;

               case 2:
                  core_type = trimSpaces(*param_it);
                  break;

               case 3:
                  l1_icache_type = trimSpaces(*param_it);
                  break;

               case 4:
                  l1_dcache_type = trimSpaces(*param_it);
                  break;

               case 5:
                  l2_cache_type = trimSpaces(*param_it);
                  break;

               default:
                  fprintf(stderr, "Tuple encountered with (%i) parameters\n", param_num);
                  exit(EXIT_FAILURE);
                  break;
            }
         }
         param_num ++;
      }

      // Append these values to an internal list
      for (UInt32 i = num_initialized_cores; i < num_initialized_cores + num_cores; i++)
      {
         m_core_parameters_vec.push_back(CoreParameters(core_type, frequency, \
                  l1_icache_type, l1_dcache_type, l2_cache_type));
      }
      num_initialized_cores += num_cores;

      if (num_initialized_cores > getTotalCores())
      {
         fprintf(stderr, "num initialized cores(%u), num cores(%u)\n",
            num_initialized_cores, getTotalCores());
         exit(EXIT_FAILURE);
      }
   }
   
   if (num_initialized_cores != getTotalCores())
   {
      fprintf(stderr, "num initialized cores(%u), num total cores(%u)\n",
         num_initialized_cores, getTotalCores());
      exit(EXIT_FAILURE);
   }
}

void Config::parseNetworkParameters()
{
   const string DEFAULT_NETWORK_TYPE = "magic";
   const float DEFAULT_FREQUENCY = 1;           // In GHz

   string network_parameters_list[NUM_STATIC_NETWORKS];
   try
   {
      config::Config *cfg = Sim()->getCfg();
      network_parameters_list[STATIC_NETWORK_USER_1] = cfg->getString("network/user_model_1");
      network_parameters_list[STATIC_NETWORK_USER_2] = cfg->getString("network/user_model_2");
      network_parameters_list[STATIC_NETWORK_MEMORY_1] = cfg->getString("network/memory_model_1");
      network_parameters_list[STATIC_NETWORK_MEMORY_2] = cfg->getString("network/memory_model_2");
      network_parameters_list[STATIC_NETWORK_SYSTEM] = cfg->getString("network/system_model");
   }
   catch (...)
   {
      fprintf(stderr, "Unable to read network parameters from the cfg file\n");
      exit(EXIT_FAILURE);
   }

   for (SInt32 i = 0; i < NUM_STATIC_NETWORKS; i++)
   {
      m_network_parameters_vec.push_back(NetworkParameters(network_parameters_list[i], DEFAULT_FREQUENCY));
   }
}

string Config::getCoreType(core_id_t core_id)
{
   LOG_ASSERT_ERROR(core_id < ((SInt32) getTotalCores()),
         "core_id(%i), total cores(%u)", core_id, getTotalCores());
   LOG_ASSERT_ERROR(m_core_parameters_vec.size() == getTotalCores(),
         "m_core_parameters_vec.size(%u), total cores(%u)",
         m_core_parameters_vec.size(), getTotalCores());

   return m_core_parameters_vec[core_id].getType();
}

string Config::getL1ICacheType(core_id_t core_id)
{
   LOG_ASSERT_ERROR(core_id < ((SInt32) getTotalCores()),
         "core_id(%i), total cores(%u)", core_id, getTotalCores());
   LOG_ASSERT_ERROR(m_core_parameters_vec.size() == getTotalCores(),
         "m_core_parameters_vec.size(%u), total cores(%u)",
         m_core_parameters_vec.size(), getTotalCores());

   return m_core_parameters_vec[core_id].getL1ICacheType();
}

string Config::getL1DCacheType(core_id_t core_id)
{
   LOG_ASSERT_ERROR(core_id < ((SInt32) getTotalCores()),
         "core_id(%i), total cores(%u)", core_id, getTotalCores());
   LOG_ASSERT_ERROR(m_core_parameters_vec.size() == getTotalCores(),
         "m_core_parameters_vec.size(%u), total cores(%u)",
         m_core_parameters_vec.size(), getTotalCores());

   return m_core_parameters_vec[core_id].getL1DCacheType();
}

string Config::getL2CacheType(core_id_t core_id)
{
   LOG_ASSERT_ERROR(core_id < ((SInt32) getTotalCores()),
         "core_id(%i), total cores(%u)", core_id, getTotalCores());
   LOG_ASSERT_ERROR(m_core_parameters_vec.size() == getTotalCores(),
         "m_core_parameters_vec.size(%u), total cores(%u)",
         m_core_parameters_vec.size(), getTotalCores());

   return m_core_parameters_vec[core_id].getL2CacheType();
}

volatile float Config::getCoreFrequency(core_id_t core_id)
{
   LOG_ASSERT_ERROR(core_id < ((SInt32) getTotalCores()),
         "core_id(%i), total cores(%u)", core_id, getTotalCores());
   LOG_ASSERT_ERROR(m_core_parameters_vec.size() == getTotalCores(),
         "m_core_parameters_vec.size(%u), total cores(%u)",
         m_core_parameters_vec.size(), getTotalCores());

   return m_core_parameters_vec[core_id].getFrequency();
}

void Config::setCoreFrequency(core_id_t core_id, volatile float frequency)
{
   LOG_ASSERT_ERROR(core_id < ((SInt32) getTotalCores()),
         "core_id(%i), total cores(%u)", core_id, getTotalCores());
   LOG_ASSERT_ERROR(m_core_parameters_vec.size() == getTotalCores(),
         "m_core_parameters_vec.size(%u), total cores(%u)",
         m_core_parameters_vec.size(), getTotalCores());

   return m_core_parameters_vec[core_id].setFrequency(frequency);
}

string Config::getNetworkType(SInt32 network_id)
{
   LOG_ASSERT_ERROR(m_network_parameters_vec.size() == NUM_STATIC_NETWORKS,
         "m_network_parameters_vec.size(%u), NUM_STATIC_NETWORKS(%u)",
         m_network_parameters_vec.size(), NUM_STATIC_NETWORKS);

   return m_network_parameters_vec[network_id].getType();
}

UInt32 Config::getNearestAcceptableCoreCount(UInt32 core_count)
{
   UInt32 nearest_acceptable_core_count = 0;
   
   for (UInt32 i = 0; i < NUM_STATIC_NETWORKS; i++)
   {
      UInt32 network_model = NetworkModel::parseNetworkType(Config::getSingleton()->getNetworkType(i));
      pair<bool,SInt32> core_count_constraints = NetworkModel::computeCoreCountConstraints(network_model, (SInt32) core_count);
      if (core_count_constraints.first)
      {
         // Network Model has core count constraints
         if ((nearest_acceptable_core_count != 0) && 
             (core_count_constraints.second != (SInt32) nearest_acceptable_core_count))
         {
            fprintf(stderr, "Problem using the network models specified in the configuration file\n");
            exit(EXIT_FAILURE);
         }
         else
         {
            nearest_acceptable_core_count = core_count_constraints.second;
         }
      }
   }

   if (nearest_acceptable_core_count == 0)
      nearest_acceptable_core_count = core_count;

   return nearest_acceptable_core_count;
}
