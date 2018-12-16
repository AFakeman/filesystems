#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <vector>

#include "BTree.hpp"

const size_t kTestElements = 1024;

void test_insert() {
  BTree<int> tree;
  std::vector<std::pair<std::string, int>> elements;
  for (size_t i = 0; i < kTestElements; ++i) {
    elements.emplace_back(std::to_string(i), i);
  }
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(elements.begin(), elements.end(), g);
  for (auto pair : elements) {
    tree.Insert(pair.first, pair.second);
  }
  for (auto pair : elements) {
    assert(tree.Contains(pair.first));
    assert(tree.Get(pair.first) == pair.second);
  }
}

void const_tests(const BTree<int> &tree) {
  for (auto i : tree) {
    std::cout << i.key << ", " << i.value.value() << std::endl;
  }
  assert(tree.Contains("0"));
  assert(tree.Get("0") == 0);
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
  const_tests(tree_3);
}

int main() {
  test_insert();
  test_merge();
  return 0;
}
