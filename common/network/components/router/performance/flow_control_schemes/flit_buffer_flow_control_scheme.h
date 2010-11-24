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
   public:
      FlitBufferFlowControlScheme(SInt32 num_input_channels, SInt32 num_output_channels);
      ~FlitBufferFlowControlScheme();
      
      // Dividing and coalescing packet at start and end
      static void dividePacket(NetPacket* net_packet, list<NetPacket*>& net_packet_list, \
            SInt32 num_flits, core_id_t requester);
      static bool isPacketComplete(NetPacket* net_packet);
   
   protected:
      class FlitBuffer
      {
         private:
            BufferModel* _buffer;

         public:
            FlitBuffer(BufferManagementScheme::Type buffer_management_scheme, \
                  SInt32 size_buffer);
            ~FlitBuffer();

            // One endpoint list per packet
            ChannelEndpointList* _output_endpoint_list;

            BufferModel* getBufferModel()
            { return _buffer; }
            
            // FIXME: Dont know if there is a better way to do this
            // Just want to call the public functions of BufferModel
            BufferManagementMsg* enqueue(Flit* flit) { return _buffer->enqueue(flit); }
            BufferManagementMsg* dequeue() { return _buffer->dequeue(); }
            Flit* front() { return _buffer->front(); }
            bool empty() { return _buffer->empty(); }
            size_t size() { return _buffer->size(); }
            void updateFlitTime() { return _buffer->updateFlitTime(); }
            void updateBufferTime() { return _buffer->updateBufferTime(); }
            UInt64 getBufferTime() { return _buffer->getBufferTime(); }
      };    
};
