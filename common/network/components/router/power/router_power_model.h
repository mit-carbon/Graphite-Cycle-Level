#pragma once

#include "fixed_types.h"

class RouterPowerModel
{
public:
   class BufferAccess
   {
   public:
      enum type_t
      {
         READ = 0,
         WRITE,
         NUM_ACCESS_TYPES
      };
   };

   RouterPowerModel(UInt32 num_input_ports, UInt32 num_output_ports, UInt32 input_buffer_size, UInt32 flit_width);
   ~RouterPowerModel();

   // Update Dynamic Energy
   virtual void updateDynamicEnergyBuffer(BufferAccess::type_t buffer_access_type, \
         UInt32 num_bit_flips, UInt32 num_flits = 1) = 0;
   virtual void updateDynamicEnergyCrossbar(UInt32 num_bit_flips, UInt32 num_flits = 1) = 0;
   virtual void updateDynamicEnergySwitchAllocator(UInt32 num_requests, UInt32 num_flits = 1) = 0;
   virtual void updateDynamicEnergyClock(UInt32 num_flits = 1) = 0;
   virtual void updateDynamicEnergy(UInt32 num_bit_flips, UInt32 num_flits = 1) = 0;
   
   // Total Dynamic Energy
   virtual volatile double getDynamicEnergyBuffer() = 0; 
   virtual volatile double getDynamicEnergyCrossbar() = 0;
   virtual volatile double getDynamicEnergySwitchAllocator() = 0;
   virtual volatile double getDynamicEnergyClock() = 0;
   virtual volatile double getTotalDynamicEnergy() = 0;

   // Static Power
   virtual volatile double getStaticPowerBuffer() = 0;
   virtual volatile double getStaticPowerBufferCrossbar() = 0;
   virtual volatile double getStaticPowerSwitchAllocator() = 0;
   virtual volatile double getStaticPowerClock() = 0;
   virtual volatile double getTotalStaticPower() = 0;

   // Reset Counters
   virtual void resetCounters() = 0;

   static RouterPowerModel* create(UInt32 num_input_ports, UInt32 num_output_ports, \
         UInt32 input_buffer_size, UInt32 flit_width, bool use_orion = true);

protected:
   UInt32 _num_input_ports;
   UInt32 _num_output_ports;
   UInt32 _input_buffer_size;
   UInt32 _flit_width;
};
