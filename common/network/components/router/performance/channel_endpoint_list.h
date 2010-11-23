#pragma once

#include <vector>
using namespace std;

#include "fixed_types.h"
#include "channel.h"

class ChannelEndpointList
{
   private:
      vector<Channel::Endpoint> _channel_endpoint_list;
      vector<Channel::Endpoint>::iterator _curr_pos;

   public:
      ChannelEndpointList(const vector<Channel::Endpoint>& channel_endpoint_list):
         _channel_endpoint_list(channel_endpoint_list)
      { _curr_pos = _channel_endpoint_list.begin(); }
      ~ChannelEndpointList() {}

      Channel::Endpoint curr() 
      { return *_curr_pos; } 

      void incr()
      { 
         _curr_pos ++; 
         if (_curr_pos == _channel_endpoint_list.end())
            _curr_pos = _channel_endpoint_list.begin();
      }

      Channel::Endpoint first()
      { return _channel_endpoint_list[0]; }

      Channel::Endpoint last()
      { return _channel_endpoint_list[_channel_endpoint_list.size()-1]; }

      size_t size()
      { return _channel_endpoint_list.size(); }
};
