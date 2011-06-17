// config.h
//
// The Config class is used to set, store, and retrieve all the configurable
// parameters of the simulator.
//
// Initial creation: Sep 18, 2008 by jasonm

#pragma once

#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <cassert>
#include <stdio.h>
#include <stdlib.h>
#include "fixed_types.h"

class Config
{
private:
   class CoreParameters
   {
      private:
         std::string m_type;
         volatile float m_frequency;
         std::string m_l1_icache_type;
         std::string m_l1_dcache_type;
         std::string m_l2_cache_type;

      public:
         CoreParameters(std::string type, volatile float frequency, std::string l1_icache_type, std::string l1_dcache_type, std::string l2_cache_type):
            m_type(type),
            m_frequency(frequency),
            m_l1_icache_type(l1_icache_type),
            m_l1_dcache_type(l1_dcache_type),
            m_l2_cache_type(l2_cache_type)
         {}
         ~CoreParameters() {}

         volatile float getFrequency() { return m_frequency; }
         void setFrequency(volatile float frequency) { m_frequency = frequency; }
         std::string getType() { return m_type; }
         std::string getL1ICacheType() { return m_l1_icache_type; }
         std::string getL1DCacheType() { return m_l1_dcache_type; }
         std::string getL2CacheType() { return m_l2_cache_type; }
   };

   class NetworkParameters
   {
      private:
         std::string m_type;
         volatile float m_frequency;

      public:
         NetworkParameters(std::string type, volatile float frequency):
            m_type(type), m_frequency(frequency)
         {}
         ~NetworkParameters() {}

         volatile float getFrequency() { return m_frequency; }
         std::string getType() { return m_type; }
   };
   
public:
   enum AccuracyMode
   {
      NORMAL = 0,
      CYCLE_LEVEL,
      NUM_ACCURACY_MODES
   };

   enum ExecutionMode
   {
      FULL = 0,
      LITE,
      NATIVE,
      NUM_EXECUTION_MODES
   };

   typedef std::vector<UInt32> CoreToProcMap;
   typedef std::vector<core_id_t> CoreList;
   typedef std::vector<core_id_t>::const_iterator CLCI;
   typedef std::map<UInt32,core_id_t> CommToCoreMap;

   Config();
   ~Config();

   void loadFromFile(char* filename);
   void loadFromCmdLine();

   // Return the total number of modules in all processes
   UInt32 getTotalCores() { return m_total_cores; }
   // Get the total number of sim threads
   UInt32 getTotalSimThreads() { return m_total_sim_threads; }

   // For mapping between user-land communication id's to actual core id's
   void updateCommToCoreMap(UInt32 comm_id, core_id_t core_id);
   UInt32 getCoreFromCommId(UInt32 comm_id);

   // Get CoreId length
   UInt32 getCoreIDLength()
   { return m_core_id_length; }

   AccuracyMode getAccuracyMode()
   { return m_accuracy_mode; }
   ExecutionMode getExecutionMode()
   { return m_execution_mode; }
   void setExecutionMode(ExecutionMode mode)
   { m_execution_mode = mode; }

   // Core & Network Parameters
   std::string getCoreType(core_id_t core_id);
   std::string getL1ICacheType(core_id_t core_id);
   std::string getL1DCacheType(core_id_t core_id);
   std::string getL2CacheType(core_id_t core_id);
   volatile float getCoreFrequency(core_id_t core_id);
   void setCoreFrequency(core_id_t core_id, volatile float frequency);

   std::string getNetworkType(SInt32 network_id);

   // Knobs
   bool isSimulatingSharedMemory() const;
   bool getEnablePerformanceModeling() const;
   bool getEnablePowerModeling() const;

   // Logging
   std::string getOutputFileName() const;
   std::string formatOutputFileName(std::string filename) const;

   static Config *getSingleton();

private:
   UInt32  m_total_cores;                 // Total number of cores in all processes
   UInt32  m_total_sim_threads;           // Total Number of sim threads
   UInt32  m_core_id_length;              // Number of bytes needed to store a core_id

   std::vector<CoreParameters> m_core_parameters_vec;         // Vector holding core parameters
   std::vector<NetworkParameters> m_network_parameters_vec;   // Vector holding network parameters

   CommToCoreMap m_comm_to_core_map;

   // Accuracy & Execution Modes
   AccuracyMode m_accuracy_mode;
   ExecutionMode m_execution_mode;

   static Config *m_singleton;

   static UInt32 m_knob_total_cores;
   static UInt32 m_knob_total_sim_threads;
   static bool m_knob_simarch_has_shared_mem;
   static std::string m_knob_output_file;
   static bool m_knob_enable_performance_modeling;
   static bool m_knob_enable_power_modeling;

   // Get Core & Network Parameters
   void parseCoreParameters();
   void parseNetworkParameters();

   // Accuracy & Execution Modes
   static AccuracyMode parseAccuracyMode(std::string mode);
   static ExecutionMode parseExecutionMode(std::string mode);

   static UInt32 computeCoreIDLength(UInt32 core_count);
   static UInt32 getNearestAcceptableCoreCount(UInt32 core_count);
};
