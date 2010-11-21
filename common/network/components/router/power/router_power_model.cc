#include "router_power_model.h"
#include "router_power_model_orion.h"
#include "log.h"

RouterPowerModel::RouterPowerModel(UInt32 num_input_ports, UInt32 num_output_ports, \
      UInt32 input_buffer_size, UInt32 flit_width):
   _num_input_ports(num_input_ports),
   _num_output_ports(num_output_ports),
   _input_buffer_size(input_buffer_size),
   _flit_width(flit_width)
{}

RouterPowerModel::~RouterPowerModel()
{}

RouterPowerModel*
RouterPowerModel::create(UInt32 num_input_ports, UInt32 num_output_ports, \
      UInt32 input_buffer_size, UInt32 flit_width, bool use_orion)
{
   LOG_ASSERT_ERROR(use_orion, "Only Orion has electrical router models at this point of time");
   return new RouterPowerModelOrion(num_input_ports, num_output_ports, input_buffer_size, flit_width);
}
