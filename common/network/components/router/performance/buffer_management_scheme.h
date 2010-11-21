#pragma once

#include <string>
using namespace std;

class BufferManagementScheme
{
   public:
      enum Type
      {
         INFINITE = 0,
         CREDIT,
         ON_OFF,
         NUM_BUFFER_MANAGEMENT_SCHEMES
      };

      static Type parse(string buffer_management_scheme_str);
};
