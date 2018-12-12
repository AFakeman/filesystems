#include <cassert>
#include <iostream>

#include "BTree.hpp"

const size_t kTestElements = 1024;

int main() {
  BTree<int> tree;
  for (size_t i = 0; i < kTestElements; ++i) {
    tree.Insert(std::to_string(i), i);
  }
  for (size_t i = 0; i < kTestElements; ++i) {
    assert(tree.Contains(std::to_string(i)));
    assert(tree.Get(std::to_string(i)) == i);
  }
  return 0;
}
