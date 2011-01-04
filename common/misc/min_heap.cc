#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <cassert>
#include <stdio.h>

#include "min_heap.h"

MinHeap::MinHeap()
{}

MinHeap::~MinHeap()
{}

bool
MinHeap::insert(Node* node)
{
   node->_index = _node_list.size();
   UInt64 key = node->_key;
   node->_key = UINT64_MAX;
   _node_list.push_back(node);
   return decreaseKey(node, key);
}

bool
MinHeap::insert(UInt64 key, void* data)
{
   Node* node = new Node(UINT64_MAX, data, _node_list.size());
   _node_list.push_back(node);
   return decreaseKey(node, key);
}

pair<UInt64,void*>
MinHeap::min()
{
   if (_node_list.empty())
      return make_pair<UInt64,void*>(UINT64_MAX, NULL);
   else
      return make_pair<UInt64,void*>(_node_list.front()->_key, _node_list.front()->_data);
}

pair<UInt64,void*>
MinHeap::extractMin()
{
   if (_node_list.empty())
      return make_pair<UInt64,void*>(UINT64_MAX, NULL);

   // Get the node with the minimum key
   Node* min_node = _node_list.front();
   swap(_node_list.front(), _node_list.back());
   _node_list.pop_back();

   // Place the swapped key correctly in the heap
   if (!_node_list.empty())
   {
      Node* last_node = _node_list.front();
      UInt64 last_node_key = last_node->_key;
      last_node->_key = 0;
      increaseKey(last_node, last_node_key);
   }

   // Return Results
   UInt64 min_node_key = min_node->_key;
   void* min_node_data = min_node->_data;
   delete min_node;

   return make_pair<UInt64,void*>(min_node_key, min_node_data);
}

bool
MinHeap::updateKey(Node* node, UInt64 key)
{
   if (node->_key < key)
      return increaseKey(node, key);
   else if (node->_key > key)
      return decreaseKey(node, key);
   else
      return false;
}

bool
MinHeap::increaseKey(Node* node, UInt64 key)
{
   assert(node->_key <= key);

   // Get child with minimum key
   Node* min_child = minChild(node->_index);
   if (min_child && (min_child->_key < key))
   {
      swap(min_child, node);
      increaseKey(node, key);
      return (min_child->_index == 0) ? true : false;
   }
   else
   {
      // Assign Key to node
      node->_key = key;
      return false;
   }
}

bool
MinHeap::decreaseKey(Node* node, UInt64 key)
{
   assert(node->_key >= key);

   // Compare the key to the parent_node's key
   Node* parent_node = parent(node->_index);
   if (parent_node && (parent_node->_key > key))
   {
      // The indices are swapped
      swap(parent_node, node);
      return decreaseKey(node, key);
   }
   else
   {
      // Assign the key
      node->_key = key;
      return (node->_index == 0) ? true : false;
   }
}

void
MinHeap::print()
{
   for (UInt32 i = 0; i < _node_list.size(); i++)
      printf("(%i, %llu, %p) - ", _node_list[i]->_index, \
            (long long unsigned int) _node_list[i]->_key, _node_list[i]->_data);
   printf("\n");
}

MinHeap::Node*
MinHeap::parent(SInt32 index)
{
   if (index > 0)
      return _node_list[(index-1)/2];
   else
      return (Node*) NULL;
}

MinHeap::Node*
MinHeap::leftChild(SInt32 index)
{
   SInt32 left_child_index = 2 * index + 1;
   if (left_child_index < (SInt32) _node_list.size())
      return _node_list[left_child_index];
   else
      return (Node*) NULL;
}

MinHeap::Node*
MinHeap::rightChild(SInt32 index)
{
   SInt32 right_child_index = 2 * index + 2;
   if (right_child_index < (SInt32) _node_list.size())
      return _node_list[right_child_index];
   else
      return (Node*) NULL;
}

MinHeap::Node*
MinHeap::minChild(SInt32 index)
{
   Node* left_child = leftChild(index);
   Node* right_child = rightChild(index);
   if (left_child)
   {
      if (right_child)
         return (left_child->_key < right_child->_key) ? left_child : right_child;
      else
         return left_child;
   }
   assert(!right_child);
   return (Node*) NULL;
}

void
MinHeap::swap(Node* node1, Node* node2)
{
   _node_list[node1->_index] = node2;
   _node_list[node2->_index] = node1;
   SInt32 temp_index = node2->_index;
   node2->_index = node1->_index;
   node1->_index = temp_index;
}

MinHeap::Node::Node(UInt64 key, void* data, SInt32 index):
   _key(key),
   _data(data),
   _index(index)
{}

MinHeap::Node::~Node()
{}

#ifdef MIN_HEAP_DEBUG

int main(int argc, char* argv[])
{
   MinHeap min_heap;
   UInt64 keys[] = {100, UINT64_MAX, 50, 35, 1000, 55};
   printf("\n\n");
   for (int i = 0; i < 6; i++)
   {
      min_heap.insert(keys[i]);
      min_heap.print();
   }

   printf("\n\n");
   for (int i = 0; i < 10; i++)
   {
      pair<UInt64,void*> key_data_pair = min_heap.extractMin();
      min_heap.print();
      printf("%llu, %p\n", key_data_pair.first, key_data_pair.second);
      printf("\n\n");
   }

   vector<MinHeap::Node*> nodes;
   MinHeap min_heap_2;
   for (int i = 0; i < 6; i++)
   {
      MinHeap::Node* node = new MinHeap::Node(keys[i]);
      nodes.push_back(node);
      min_heap_2.insert(node);
   }

   min_heap_2.decreaseKey(nodes[1], 10);
   min_heap_2.print();
   min_heap_2.increaseKey(nodes[3], UINT64_MAX);
   min_heap_2.print();
   min_heap_2.decreaseKey(nodes[3], 45);
   min_heap_2.print();
   return 0;
}

#endif
