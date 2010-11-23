#pragma once

#include "fixed_types.h"

class Channel
{
   public:
      class Endpoint
      {
         public:
            Endpoint():
               _channel_id(-1), _index(-1) {}
            Endpoint(SInt32 channel_id, SInt32 index = ALL): 
               _channel_id(channel_id), _index(index) {}
            
            ~Endpoint() {}

            bool operator==(const Endpoint& endpoint) const
            { return ((_channel_id == endpoint._channel_id) && (_index == endpoint._index)); }

            SInt32 _channel_id;
            SInt32 _index;
            const static SInt32 ALL = 0xdeadbeef;
      };

      const static SInt32 INVALID = 0xbabecafe;
};
