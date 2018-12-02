#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

template <class T> class BTree {
public:
  BTree();
  void Insert(const std::string &key, const T &value);
  T &Get(const std::string &key);
  const T &Get(const std::string &key) const;
  void Pop(const std::string &key);
  bool Contains(const std::string &key);

private:
  static const size_t block_size = 16;

  template <class DataType> struct BaseNode {
    std::string key;
    DataType value;
  };

  template <class DataType> struct BaseBlock {
    std::vector<BaseNode<DataType>> nodes;
    std::unique_ptr<BaseBlock<DataType>> next;
  };

  typedef std::optional<T> DataType;
  typedef BaseBlock<DataType> DataBlock;
  typedef BaseNode<DataType> DataNode;

  struct NodeBlock;
  typedef std::variant<std::unique_ptr<DataBlock>, std::unique_ptr<NodeBlock>>
      BlockPointer;
  struct NodeBlock : public BaseBlock<BlockPointer> {};
  typedef BaseNode<BlockPointer> Node;

  std::optional<std::vector<Node>>
  InsertInNode(NodeBlock *node, const std::string &key, const T &value);

  std::optional<std::vector<DataNode>>
  InsertInDataNode(DataBlock *node, const std::string &key, const T &value);

  template <class DType>
  std::optional<std::vector<BaseNode<DType>>>
  InsertMaybeSplit(std::vector<BTree<T>::BaseNode<DType>> &vec, const std::string &key, DType &&value);

  DataType *FindInNode(NodeBlock *node, const std::string &key, const T &value);
  DataType *FindInDataNode(DataBlock *node, const std::string &key, const T &value);

  BlockPointer root_;
};

template <typename T>
BTree<T>::BTree() : root_(std::make_unique<DataBlock>(DataBlock{{{"", {}}}, nullptr})){}

template <typename T>
void BTree<T>::Insert(const std::string &key, const T &value) {
  if (std::holds_alternative<std::unique_ptr<NodeBlock>>(root_)) {
    NodeBlock *child = std::get<std::unique_ptr<NodeBlock>>(root_).get();
    auto split = InsertInNode(child, key, value);
    if (!split) {
      return;
    }
    root_ = std::make_unique<NodeBlock>(NodeBlock{std::move(split.value()), nullptr});
  } else if (std::holds_alternative<std::unique_ptr<DataBlock>>(root_)) {
    DataBlock *child = std::get<std::unique_ptr<DataBlock>>(root_).get();
    auto split = InsertInDataNode(child, key, value);
    if (!split) {
      return;
    }
    root_ = std::make_unique<DataBlock>(DataBlock{std::move(split.value()), nullptr});
  }
}

template <typename T>
bool BTree<T>::Contains(const std::string &key) {

}

template <typename T>
typename BTree<T>::DataType *
BTree<T>::FindInNode(NodeBlock *node, const std::string &key, const T &value) {
  auto iter = std::upper_bound(
      node->nodes.begin(), node->nodes.end(), key,
      [](const std::string &lhs, const Node &rhs) { return lhs < rhs.key; });
}

template <typename T>
typename BTree<T>::DataType *BTree<T>::FindInDataNode(DataBlock *node, const std::string &key, const T &value) {
}

template <typename T>
template <class DType>
std::optional<std::vector<typename BTree<T>::template BaseNode<DType>>>
BTree<T>::InsertMaybeSplit(std::vector<BTree<T>::BaseNode<DType>> &vec, const std::string &key, DType &&value) {
  auto iter = std::lower_bound(
      vec.begin(), vec.end(), key,
      [](const BaseNode<DType> &lhs, const std::string &rhs) { return lhs.key < rhs; });

  if (iter != vec.end() && iter->key == key) {
    iter->value = std::move(value);
    return {};
  }

  vec.emplace(iter, BaseNode<DType>{key, std::move(value)});
  if (vec.size() <= block_size) {
    return {};
  }

  std::vector<BTree<T>::BaseNode<DType>> rest;
  rest.insert(rest.end(), std::make_move_iterator(vec.begin() + vec.size() / 2),
                          std::make_move_iterator(vec.end()));
  vec.resize(vec.size() / 2);
  return rest;
}

template <typename T>
std::optional<std::vector<typename BTree<T>::Node>>
BTree<T>::InsertInNode(NodeBlock *node, const std::string &key,
                       const T &value) {

  auto iter = std::upper_bound(
      node->nodes.begin(), node->nodes.end(), key,
      [](const std::string &lhs, const Node &rhs) { return lhs < rhs.key; });

  iter--;
  BlockPointer child_ptr;
  std::string new_key;
  if (std::holds_alternative<std::unique_ptr<NodeBlock>>(iter->value)) {
    NodeBlock *child = std::get<std::unique_ptr<NodeBlock>>(iter->value).get();
    auto split = InsertInNode(child, key, value);
    if (!split) {
      return {};
    }
    new_key = split.value()[0].key;
    NodeBlock *new_block = new NodeBlock{std::move(split.value()), nullptr};
    child_ptr = std::unique_ptr<NodeBlock>(new_block);
  } else if (std::holds_alternative<std::unique_ptr<DataBlock>>(iter->value)) {
    DataBlock *child = std::get<std::unique_ptr<DataBlock>>(iter->value).get();
    auto split = InsertInDataNode(child, key, value);
    if (!split) {
      return {};
    }
    new_key = split.value()[0].key;
    DataBlock *new_block = new DataBlock{std::move(split.value()), nullptr};
    child_ptr = std::unique_ptr<DataBlock>(new_block);
  }
  return InsertMaybeSplit(node->nodes, new_key, std::move(child_ptr));
}

template <typename T>
std::optional<std::vector<typename BTree<T>::DataNode>>
BTree<T>::InsertInDataNode(DataBlock *node, const std::string &key,
                           const T &value) {
  return InsertMaybeSplit(node->nodes, key, {value});
}
