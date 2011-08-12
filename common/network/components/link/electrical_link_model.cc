#include "electrical_link_model.h"
#include "log.h"

ElectricalLinkModel::ElectricalLinkModel(volatile float link_frequency, \
      volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints):
   LinkModel(link_frequency, link_length, link_width, num_receiver_endpoints)
{}

ElectricalLinkModel::~ElectricalLinkModel()
{}

ElectricalLinkModel::Type
ElectricalLinkModel::parse(std::string link_type_str)
{
   if (link_type_str == "electrical_repeated")
      return REPEATED;
   else if (link_type_str == "electrical_equalized")
      return EQUALIZED;
   else
   {
      LOG_PRINT_ERROR("Unrecognized Link Type(%s)", link_type_str.c_str());
      return NUM_LINK_TYPES;
   }
}
