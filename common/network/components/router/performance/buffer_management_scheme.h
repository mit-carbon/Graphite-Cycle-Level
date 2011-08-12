#pragma once

#include <string>

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

      static Type parse(std::string buffer_management_scheme_str);
      static std::string getTypeString(Type type);
};
