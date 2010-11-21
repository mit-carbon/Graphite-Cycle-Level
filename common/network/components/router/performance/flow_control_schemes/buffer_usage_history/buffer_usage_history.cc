#include "buffer_usage_history.h"
#include "infinite_buffer_usage_history.h"
#include "credit_history.h"
#include "on_off_history.h"
#include "log.h"

BufferUsageHistory*
BufferUsageHistory::create(BufferManagementScheme::Type buffer_management_scheme, \
      SInt32 size_buffer)
{
   switch(buffer_management_scheme)
   {
      case (BufferManagementScheme::INFINITE):
         return new InfiniteBufferUsageHistory();

      case BufferManagementScheme::CREDIT:
         return new CreditHistory(size_buffer);

      case BufferManagementScheme::ON_OFF:
         return new OnOffHistory(size_buffer);

      default:
         LOG_PRINT_ERROR("Unrecognized Buffer Management Scheme(%u)", \
               buffer_management_scheme);
         return (BufferUsageHistory*) NULL;
   }
}
