#include "buffer_status.h"
#include "infinite_buffer_status.h"
#include "credit_status.h"
#include "on_off_status.h"
#include "log.h"

BufferStatus*
BufferStatus::create(BufferManagementScheme::Type buffer_management_scheme,
      SInt32 size_buffer)
{
   switch(buffer_management_scheme)
   {
      case (BufferManagementScheme::INFINITE):
         return new InfiniteBufferStatus();

      case BufferManagementScheme::CREDIT:
         return new CreditStatus(size_buffer);

      case BufferManagementScheme::ON_OFF:
         return new OnOffStatus(size_buffer);

      default:
         LOG_PRINT_ERROR("Unrecognized Buffer Management Scheme(%u)",
               buffer_management_scheme);
         return (BufferStatus*) NULL;
   }
}
