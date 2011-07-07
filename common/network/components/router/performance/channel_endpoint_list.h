#pragma once

#include <vector>
#include <cassert>
using namespace std;

#include "fixed_types.h"
#include "channel.h"

class ChannelEndpointList
{
   private:
      vector<Channel::Endpoint> _channel_endpoint_vec;
      vector<Channel::Endpoint>::iterator _curr_pos;

   public:
      ChannelEndpointList(const vector<Channel::Endpoint>& channel_endpoint_vec):
         _channel_endpoint_vec(channel_endpoint_vec)
      { _curr_pos = _channel_endpoint_vec.begin(); }
      ~ChannelEndpointList() {}

      Channel::Endpoint curr()
      { assert(_curr_pos != _channel_endpoint_vec.end()); return *_curr_pos; }

      void incr()
      {
         assert(_curr_pos != _channel_endpoint_vec.end());
         _curr_pos ++;
         if (_curr_pos == _channel_endpoint_vec.end())
            _curr_pos = _channel_endpoint_vec.begin();
      }

      Channel::Endpoint first()
      { assert(!empty()); return _channel_endpoint_vec[0]; }

      Channel::Endpoint last()
      { assert(!empty()); return _channel_endpoint_vec[_channel_endpoint_vec.size()-1]; }

      size_t size()
      { return _channel_endpoint_vec.size(); }

      bool empty()
      { return _channel_endpoint_vec.empty(); }
};
