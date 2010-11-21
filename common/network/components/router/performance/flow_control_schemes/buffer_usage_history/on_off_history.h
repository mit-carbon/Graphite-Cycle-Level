#pragma once

#include <list>
using namespace std;

#include "on_off_msg.h"
#include "finite_buffer_usage_history.h"

class OnOffHistory : public FiniteBufferUsageHistory
{
   public:
      OnOffHistory(SInt32 size_buffer);
      ~OnOffHistory();

      bool allocate(Flit* flit);
      void prune(UInt64 time);
      void receive(BufferManagementMsg* buffer_mangement_msg);
   
   private:
      list<OnOffMsg*> _on_off_history;

      void updateUsage(UInt64 time);
};
