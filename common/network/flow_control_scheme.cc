#include "flow_control_scheme.h"

FlowControlScheme::FlowControlScheme(UInt32 num_input_channels, UInt32 num_output_channels, UInt32 input_queue_size):
   _num_input_channels(num_input_channels),
   _num_output_channels(num_output_channels),
   _input_queue_size(input_queue_size)
{}

FlowControlScheme::~FlowControlScheme()
{}

void
FlowControlScheme::sendNetworkMessages()
{
   vector<NetworkMessage*>::iterator it = _network_message_list.begin();
   for ( ; it != _network_message_list.end(); it++)
   {
      switch ((*it)->_type)
      {
         case NetworkMessage::DATA:
            Flit* flit = dynamic_cast<Flit*>((*it));
            _output_channel_to_node_mapping[flit->_output_channel];
            break;

         case NetworkMessage::BUFFER_MANAGEMENT:
            BufferManagementMessage* buffer_management_message = dynamic_cast<BufferManagementMessage*>((*it));
            _input_channel_to_node_mapping[buffer_management_message->_channel];
            break;

         default:
            LOG_PRINT_ERROR("Unsupported Message Type (%u)", (*it)->_type);
            break;
      }

      delete (*it);
   }
}
