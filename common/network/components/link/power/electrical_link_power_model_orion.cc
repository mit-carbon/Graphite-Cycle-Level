#include "electrical_link_power_model_orion.h"
#include "log.h"

ElectricalLinkPowerModelOrion::ElectricalLinkPowerModelOrion(ElectricalLinkModel::Type link_type, \
      volatile float link_frequency, volatile double link_length, UInt32 link_width, \
      SInt32 num_receiver_endpoints):
   ElectricalLinkPowerModel(link_frequency, link_length, link_width, num_receiver_endpoints),
   _total_dynamic_energy(0)
{
   LOG_ASSERT_ERROR(link_type == ElectricalLinkModel::REPEATED, \
         "Orion only supports REPEATED_ELECTRICAL link models currently");
   _orion_link = new OrionLink(link_length / 1000, link_width, OrionConfig::getSingleton());
}

ElectricalLinkPowerModelOrion::~ElectricalLinkPowerModelOrion()
{
   delete _orion_link;
}

volatile double
ElectricalLinkPowerModelOrion::getStaticPower()
{
   return _orion_link->get_static_power();
}

void
ElectricalLinkPowerModelOrion::updateDynamicEnergy(UInt32 num_bit_flips, UInt32 num_flits)
{
   volatile double dynamic_energy = _orion_link->calc_dynamic_energy(num_bit_flips);
   _total_dynamic_energy += (num_flits * dynamic_energy);
}
