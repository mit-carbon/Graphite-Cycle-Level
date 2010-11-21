#include "buffer_management_scheme.h"
#include "log.h"

BufferManagementScheme::Type
BufferManagementScheme::parse(string buffer_management_scheme_str)
{
   if (buffer_management_scheme_str == "infinite")
      return INFINITE;
   else if (buffer_management_scheme_str == "credit")
      return CREDIT;
   else if (buffer_management_scheme_str == "on_off")
      return ON_OFF;
   else
   {
      LOG_PRINT_ERROR("Unrecognized Buffer Management Scheme(%s)", \
            buffer_management_scheme_str.c_str());
      return NUM_BUFFER_MANAGEMENT_SCHEMES;
   }
}
