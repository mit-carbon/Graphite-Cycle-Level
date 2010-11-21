#pragma once

#include <list>
using namespace std;

#include "fixed_types.h"
#include "buffer_model.h"
#include "channel_endpoint_list.h"
#include "buffer_management_scheme.h"
#include "buffer_management_msg.h"
#include "buffer_usage_history.h"
#include "flit.h"
#include "flow_control_scheme.h"

class FlitBufferFlowControlScheme : public FlowControlScheme
{
   protected:
      class FlitBuffer
      {
         private:
            BufferModel* _buffer;

         public:
            FlitBuffer(BufferManagementScheme::Type buffer_management_scheme, \
                  SInt32 size_buffer);
            ~FlitBuffer();

            ChannelEndpointList* _output_endpoint_list;

            // FIXME: Dont know if there is a better way to do this
            // Just want to call the public functions of BufferModel
            BufferManagementMsg* enqueue(Flit* flit) { return _buffer->enqueue(flit); }
            BufferManagementMsg* dequeue() { return _buffer->dequeue(); }
            Flit* front() { return _buffer->front(); }
            bool empty() { return _buffer->empty(); }
            size_t size() { return _buffer->size(); }
            void updateFlitTime() { return _buffer->updateFlitTime(); }
            void updateBufferTime() { return _buffer->updateBufferTime(); }
      };

      static SInt32 computeNumFlits(SInt32 packet_length, SInt32 flit_width);

   public:
      FlitBufferFlowControlScheme(SInt32 num_input_channels, SInt32 num_output_channels);
      ~FlitBufferFlowControlScheme();
      
      static void dividePacket(NetPacket* net_packet, list<NetPacket*>& net_packet_list, \
            SInt32 packet_length, SInt32 flit_width);
      static bool isPacketComplete(NetPacket* net_packet);
};
