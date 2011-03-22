#define TRANSPORT_CC

#include <assert.h>

#include "transport.h"
#include "socktransport.h"

#include "config.h"
#include "log.h"

// -- Transport -- //

Transport *Transport::m_singleton;

Transport::Transport()
{
}

Transport* Transport::create()
{
   assert(m_singleton == NULL);

   if (true)
      m_singleton = new SockTransport();
   else
      LOG_PRINT_ERROR("Negative no. processes ?!");

   return m_singleton;
}

Transport* Transport::getSingleton()
{
   return m_singleton;
}

// -- Node -- //

Transport::Node::Node(SInt32 node_id)
   : m_node_id(node_id)
{
}

SInt32 Transport::Node::getNodeId()
{
   return m_node_id;
}
