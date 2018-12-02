#include <cassert>

#include "BTree.hpp"

int main() {
  BTree<int> tree;
  tree.Insert("hello", 2);
  assert(tree.Contains("hello"));
  assert(!tree.Contains("hllo"));
  tree.Pop("hello");
  assert(!tree.Contains("hello"));
  return 0;
}
