#pragma once

#include "fixed_types.h"

class Router
{
public:
   class Id
   {
   public:
      Id(): _core_id(INVALID_CORE_ID), _index(-1) {}
      Id(core_id_t core_id, SInt32 index):
         _core_id(core_id), _index(index) {}
      ~Id() {}

      bool operator==(const Id& router_id) const
      { return ((_core_id == router_id._core_id) && (_index == router_id._index)); }
      bool operator!=(const Id& router_id) const
      { return ((_core_id != router_id._core_id) || (_index != router_id._index)); }
      bool operator<(const Id& router_id) const
      { 
         return ( (_core_id < router_id._core_id) || \
                 ((_core_id == router_id._core_id) && (_index < router_id._index)) ) ; 
      }

      core_id_t _core_id;
      SInt32 _index;
   };
};
