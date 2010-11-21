#pragma once

#include <list>
using namespace std;

#include "credit_msg.h"
#include "finite_buffer_usage_history.h"

class CreditHistory : public FiniteBufferUsageHistory
{
   public:
      CreditHistory(SInt32 size_buffer);
      ~CreditHistory();

      bool allocate(Flit* flit);
      void prune(UInt64 time);
      void receive(BufferManagementMsg* buffer_mangement_msg);
   
   private:
      list<CreditMsg*> _credit_history;
      
      void decreaseCredits(UInt64 time, SInt32 num_credits);
};
