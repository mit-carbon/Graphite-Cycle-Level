#ifndef TRANSPORT_H
#define TRANSPORT_H

#include "fixed_types.h"

#include <map>

class Transport
{
public:
   virtual ~Transport() { };

   typedef std::pair<Byte*,SInt32> BufferTagPair;

   class Node
   {
   public:
      virtual ~Node() { }

      virtual void globalSend(SInt32 dest_proc, const void *buffer, UInt32 length) = 0;
      virtual void send(core_id_t dest, const void *buffer, UInt32 length) = 0;
      virtual BufferTagPair recv() = 0;
      virtual bool query() = 0;

   protected:
      SInt32 getNodeId();
      Node(SInt32 node_id);

   private:
      SInt32 m_node_id;
   };

   static Transport* create();
   static Transport* getSingleton();

   virtual Node* createNode(core_id_t core_id) = 0;

   virtual void barrier() = 0;
   virtual Node* getGlobalNode() = 0; // for communication not linked to a core

protected:
   Transport();

private:
   static Transport *m_singleton;
};

#endif // TRANSPORT_H

