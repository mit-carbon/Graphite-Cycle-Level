#include "shmem_req.h"
#include "log.h"

namespace PrL1PrL2DramDirectoryMSI
{
   ShmemReq::ShmemReq(ShmemMsg* shmem_msg, UInt64 time):
      m_time(time)
   {
      // Make a local copy of the shmem_msg
      m_shmem_msg = shmem_msg->clone();
      LOG_ASSERT_ERROR(shmem_msg->getDataBuf() == NULL, 
            "Shmem Reqs should not have data payloads");
   }

   ShmemReq::~ShmemReq()
   {
      m_shmem_msg->release();
   }
}
