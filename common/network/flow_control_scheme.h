#pragma once

#include "fixed_types.h"
#include "network_msg.h"

class FlowControlScheme
{
   public:
      enum Type
      {
         STORE_AND_FORWARD = 0,
         VIRTUAL_CUT_THROUGH,
         WORMHOLE,
         NUM_SCHEMES
      };

      FlowControlScheme(UInt32 num_input_channels, UInt32 num_output_channels, UInt32 input_queue_size):
         _num_input_channels(num_input_channels),
         _num_output_channels(num_output_channels),
         _input_queue_size(input_queue_size)
      {}
      ~FlowControlScheme()
      {}

      // Public Member Functions
      vector<NetworkMsg*> processNetworkMsg(NetworkMsg* network_msg);

   protected:
      UInt32 _num_input_channels;
      UInt32 _num_output_channels;
      UInt32 _input_queue_size;
};
