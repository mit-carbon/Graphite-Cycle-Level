#include "electrical_link_power_model.h"
#include "electrical_link_power_model_repeated.h"
#include "electrical_link_power_model_equalized.h"
#include "electrical_link_power_model_orion.h"
#include "log.h"

ElectricalLinkPowerModel::ElectricalLinkPowerModel(volatile float link_frequency, \
      volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints):
   ElectricalLinkModel(link_frequency, link_length, link_width, num_receiver_endpoints),
   LinkPowerModel()
{}

ElectricalLinkPowerModel::~ElectricalLinkPowerModel()
{}

ElectricalLinkPowerModel*
ElectricalLinkPowerModel::create(string link_type_str, volatile float link_frequency, \
      volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints, bool use_orion)
{
   ElectricalLinkModel::Type link_type = ElectricalLinkModel::parse(link_type_str);

   if (!use_orion)
   {
      switch (link_type)
      {
         case ElectricalLinkModel::REPEATED:
            return new ElectricalLinkPowerModelRepeated( \
                  link_frequency, link_length, link_width, num_receiver_endpoints);

         case ElectricalLinkModel::EQUALIZED:
            return new ElectricalLinkPowerModelEqualized( \
                  link_frequency, link_length, link_width, num_receiver_endpoints);

         default:
            LOG_PRINT_ERROR("Unrecognized Link Type(%u)", link_type);
            return (ElectricalLinkPowerModel*) NULL;
      }
   }
   else
   {
      return new ElectricalLinkPowerModelOrion(link_type, \
            link_frequency, link_length, link_width, num_receiver_endpoints);
   }
}
