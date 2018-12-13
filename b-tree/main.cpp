#include <cassert>
#include <iostream>

#include "BTree.hpp"

const size_t kTestElements = 1024;

void test_insert() {
  BTree<int> tree;
  for (size_t i = 0; i < kTestElements; ++i) {
    tree.Insert(std::to_string(i), i);
  }
  for (size_t i = 0; i < kTestElements; ++i) {
    assert(tree.Contains(std::to_string(i)));
    assert(tree.Get(std::to_string(i)) == i);
  }
}

void test_merge() {
  BTree<int> tree_1;
  BTree<int> tree_2;
  for (size_t i = 0; i < kTestElements; ++i) {
    tree_1.Insert(std::to_string(i), i);
    tree_2.Insert(std::to_string(i + kTestElements), i + kTestElements);
  }
  auto tree_3 = BTree<int>::Merge(tree_1, tree_2);
  assert(tree_3.size() == (tree_1.size() + tree_2.size()));
  for(auto i : tree_1) {
    assert(tree_3.Contains(i.key));
  }
  for(auto i : tree_2) {
    assert(tree_3.Contains(i.key));
  }
}

int main() {
  test_insert();
  test_merge();
  return 0;
}
