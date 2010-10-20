#include "store_and_forward_flow_control_scheme.h"

StoreAndForwardFlowControlScheme::StoreAndForwardFlowControlScheme(UInt32 num_input_channels, \
      UInt32 num_output_channels, UInt32 input_queue_size):
   PacketBufferFlowControlScheme(num_input_channels, num_output_channels, input_queue_size)
{}

StoreAndForwardFlowControlScheme::~StoreAndForwardFlowControlScheme()
{}

void
StoreAndForwardFlowControlScheme::processDataMsg(Flit* flit)
{
   assert(flit->_type == Flit::HEAD);
   HeadFlit& head_flit = *((HeadFlit*) flit);
   head_flit._time += (head_flit._length - 1);
   
   PacketBufferFlowControlScheme::processDataMsg(flit);
}
