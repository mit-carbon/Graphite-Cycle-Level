#pragma once

#include "mem_component.h"
#include "fixed_types.h"

namespace PrL1PrL2DramDirectoryMSI
{
   class ShmemMsg
   {
   public:
      enum msg_t
      {
         INVALID_MSG_TYPE = 0,
         MIN_MSG_TYPE,
         EX_REQ = MIN_MSG_TYPE,
         SH_REQ,
         INV_REQ,
         FLUSH_REQ,
         WB_REQ,
         EX_REP,
         SH_REP,
         UPGRADE_REP,
         INV_REP,
         FLUSH_REP,
         WB_REP,
         NULLIFY_REQ,
         GET_DATA_REQ,
         PUT_DATA_REQ,
         GET_DATA_REP,
         MAX_MSG_TYPE = GET_DATA_REP,
         NUM_MSG_TYPES = MAX_MSG_TYPE - MIN_MSG_TYPE + 1
      };  

   private:   
      msg_t m_msg_type;
      MemComponent::component_t m_sender_mem_component;
      MemComponent::component_t m_receiver_mem_component;
      core_id_t m_requester;
      IntPtr m_address;
      bool m_reply_expected;
      Byte* m_data_buf;
      UInt32 m_data_length;

   public:
      ShmemMsg();
      ShmemMsg(msg_t msg_type,
            MemComponent::component_t sender_mem_component,
            MemComponent::component_t receiver_mem_component,
            core_id_t requester,
            IntPtr address,
            bool reply_expected,
            Byte* data_buf,
            UInt32 data_length);
      ShmemMsg(const ShmemMsg& shmem_msg);

      ~ShmemMsg();

      static ShmemMsg* getShmemMsg(Byte* msg_buf);
      Byte* makeMsgBuf() const;
      UInt32 getMsgLen() const;
      ShmemMsg* clone() const;
      void release();

      // Modeling
      UInt32 getModeledLength() const;

      msg_t getMsgType() const { return m_msg_type; }
      MemComponent::component_t getSenderMemComponent() const { return m_sender_mem_component; }
      MemComponent::component_t getReceiverMemComponent() const { return m_receiver_mem_component; }
      core_id_t getRequester() const { return m_requester; }
      IntPtr getAddress() const { return m_address; }
      bool isReplyExpected() const { return m_reply_expected; }
      Byte* getDataBuf() const { return m_data_buf; }
      UInt32 getDataLength() const { return m_data_length; }

      void setDataBuf(Byte* data_buf) { m_data_buf = data_buf; }
   };
}
