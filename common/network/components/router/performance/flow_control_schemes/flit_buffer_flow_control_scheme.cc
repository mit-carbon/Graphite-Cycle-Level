#include <cmath>

#include "head_flit.h"
#include "flit_buffer_flow_control_scheme.h"

FlitBufferFlowControlScheme::FlitBufferFlowControlScheme( \
      SInt32 num_input_channels, SInt32 num_output_channels):
   FlowControlScheme(num_input_channels, num_output_channels)
{}

FlitBufferFlowControlScheme::~FlitBufferFlowControlScheme()
{}

void
FlitBufferFlowControlScheme::dividePacket(NetPacket* net_packet, \
      list<NetPacket*>& net_packet_list, \
      SInt32 packet_length, SInt32 flit_width)
{
   // net_packet.getModeledLength() includes the size of the data + header
   SInt32 num_flits = computeNumFlits(packet_length, flit_width);

   // Make HeadFlit first
   HeadFlit* head_flit = new HeadFlit(1, net_packet->sender, net_packet->receiver);
   NetPacket* head_flit_packet = new NetPacket(net_packet->time, net_packet->type, \
         sizeof(*head_flit), (void*) head_flit, \
         false /* is_raw */, net_packet->sequence_num);
   net_packet_list.push_back(head_flit_packet);

   for (SInt32 i = 1; i < num_flits - 1; i++)
   {
      Flit* body_flit = new Flit(Flit::BODY, 1);
      NetPacket* body_flit_packet = new NetPacket(net_packet->time + i, net_packet->type,
            sizeof(*body_flit), (void*) body_flit, \
            false /* is_raw */, net_packet->sequence_num);
      net_packet_list.push_back(body_flit_packet);
   }
   if (num_flits > 1)
   {
      // Have a separate TAIL flit
      Flit* tail_flit = new Flit(Flit::TAIL, 1);
      NetPacket* tail_flit_packet = new NetPacket(net_packet->time + num_flits - 1, net_packet->type, \
            sizeof(*tail_flit), (void*) tail_flit, \
            false /* is_raw */, net_packet->sequence_num);
      net_packet_list.push_back(tail_flit_packet);
   }
   else
   {
      // The head flit is also the tail flit
      head_flit->_type = (Flit::Type) ( ((SInt32) head_flit->_type) | ((SInt32) Flit::TAIL) );
   }
}

bool
FlitBufferFlowControlScheme::isPacketComplete(NetPacket* net_packet)
{
   assert(!net_packet->is_raw);
   NetworkMsg* network_msg = (NetworkMsg*) net_packet->data;
   assert(network_msg->_type == NetworkMsg::DATA);
   Flit* flit = (Flit*) network_msg;
   return (flit->_type & Flit::TAIL);
}

SInt32
FlitBufferFlowControlScheme::computeNumFlits(SInt32 packet_length, SInt32 flit_width)
{
   return (SInt32) ceil((float) (packet_length * 8) / flit_width);
}

FlitBufferFlowControlScheme::FlitBuffer::FlitBuffer( \
      BufferManagementScheme::Type buffer_management_scheme, \
      SInt32 size_buffer):
   _output_endpoint_list(NULL)
{
   _buffer = BufferModel::create(buffer_management_scheme, size_buffer);
}

FlitBufferFlowControlScheme::FlitBuffer::~FlitBuffer()
{
   assert(_output_endpoint_list == NULL);
   delete _buffer;
}
