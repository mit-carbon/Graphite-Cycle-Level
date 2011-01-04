#pragma once

#include <vector>
using std::pair;
using std::make_pair;
using std::vector;

#include "fixed_types.h"

class MinHeap
{
   public:
      class Node
      {
         public:
            Node(UInt64 key, void* data = NULL, SInt32 index = -1);
            ~Node();

            UInt64 _key;
            void* _data;
            SInt32 _index;
      };

      MinHeap();
      ~MinHeap();

      // Interface Functions
      bool insert(Node* node);
      bool insert(UInt64 key, void* data = NULL);
      pair<UInt64,void*> min();
      pair<UInt64,void*> extractMin();
      bool updateKey(Node* node, UInt64 key);
      bool increaseKey(Node* node, UInt64 key);
      bool decreaseKey(Node* node, UInt64 key);
      size_t size() { return _node_list.size(); }

      // Debug
      void print();

   private:
      vector<Node*> _node_list;

      Node* parent(SInt32 index);
      Node* leftChild(SInt32 index);
      Node* rightChild(SInt32 index);
      Node* minChild(SInt32 index);
      void swap(Node* node1, Node* node2);
};
