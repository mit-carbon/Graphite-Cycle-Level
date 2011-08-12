#include "electrical_link_performance_model.h"
#include "electrical_link_performance_model_repeated.h"
#include "electrical_link_performance_model_equalized.h"
#include "log.h"

ElectricalLinkPerformanceModel::ElectricalLinkPerformanceModel(volatile float link_frequency,
      volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints):
   ElectricalLinkModel(link_frequency, link_length, link_width, num_receiver_endpoints),
   LinkPerformanceModel()
{}

ElectricalLinkPerformanceModel::~ElectricalLinkPerformanceModel()
{}

ElectricalLinkPerformanceModel*
ElectricalLinkPerformanceModel::create(std::string link_type_str, volatile float link_frequency,
      volatile double link_length, UInt32 link_width, SInt32 num_receiver_endpoints)
{
   ElectricalLinkModel::Type link_type = ElectricalLinkModel::parse(link_type_str);

   switch (link_type)
   {
      case ElectricalLinkModel::REPEATED:
         return new ElectricalLinkPerformanceModelRepeated(link_frequency,
               link_length, link_width, num_receiver_endpoints);

      case ElectricalLinkModel::EQUALIZED:
         return new ElectricalLinkPerformanceModelEqualized(link_frequency,
               link_length, link_width, num_receiver_endpoints);

      default:
         LOG_PRINT_ERROR("Unrecognized Link Type(%u)", link_type);
         return (ElectricalLinkPerformanceModel*) NULL;
   }
}
