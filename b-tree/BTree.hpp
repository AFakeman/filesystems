#include <algorithm>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

template <class T> class BTree {
public:
  BTree();
  void Insert(const std::string &key, const T &value);
  T &Get(const std::string &key);
  void Pop(const std::string &key);
  bool Contains(const std::string &key);

  void PrintLeaves();
private:
  static const size_t block_size = 16;

  template <class DataType> struct BaseNode {
    std::string key;
    DataType value;
  };

  template <class DataType> struct BaseBlock {
    using NodeType = BaseNode<DataType>;
    std::vector<NodeType> nodes;
    BaseBlock *next;
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
  InsertInNode(DataBlock *node, const std::string &key, const T &value);

  template <class DType>
  std::optional<std::vector<BaseNode<DType>>>
  InsertMaybeSplit(std::vector<BTree<T>::BaseNode<DType>> &vec,
                   const std::string &key, DType &&value);

  DataType *Find(const std::string &key);
  DataType *FindInNode(NodeBlock *node, const std::string &key);
  DataType *FindInNode(DataBlock *node, const std::string &key);

  BlockPointer root_;
};

template <typename T>
BTree<T>::BTree()
    : root_(std::make_unique<DataBlock>(DataBlock{{{"", {}}}, nullptr})) {}

template <typename T>
void BTree<T>::Insert(const std::string &key, const T &value) {
  auto lambda = [this, key, value](auto &root) {
    auto *cur_root = root.get();
    using BlockType = typename std::remove_reference<decltype(*cur_root)>::type;

    auto split = InsertInNode(cur_root, key, value);
    if (!split) {
      return;
    }

    std::cout << "Splitting for key " << key << std::endl;
    // Current root will become the child
    auto new_root = std::make_unique<NodeBlock>();
    std::string split_key = split.value()[0].key;
    auto split_block = std::make_unique<BlockType>(
        BlockType{std::move(split.value()), nullptr});
    cur_root->next = split_block.get();

    // No split will be done as the new root has only half of the nodes.
    InsertMaybeSplit(new_root->nodes, cur_root->nodes[0].key,
                     {std::move(root)});
    InsertMaybeSplit(new_root->nodes, split_key, {std::move(split_block)});
    root_ = std::move(new_root);
  };

  std::visit(lambda, root_);
}

template <typename T> bool BTree<T>::Contains(const std::string &key) {
  DataType *result = Find(key);
  return result && *result;
}

template <typename T> void BTree<T>::Pop(const std::string &key) {
  DataType *item = Find(key);
  if (item) {
    item->reset();
  }
}

template <typename T> T &BTree<T>::Get(const std::string &key) {
  DataType *item = Find(key);
  if (item) {
    return item->value();
  } else {
    throw std::runtime_error("No such element");
  }
}

template <typename T>
typename BTree<T>::DataType *BTree<T>::Find(const std::string &key) {
  auto lambda = [this, key](auto &root) -> DataType * {
    return FindInNode(root.get(), key);
  };

  return std::visit(lambda, root_);
}

template <typename T>
typename BTree<T>::DataType *BTree<T>::FindInNode(NodeBlock *node,
                                                  const std::string &key) {
  auto iter = std::upper_bound(
      node->nodes.begin(), node->nodes.end(), key,
      [](const std::string &lhs, const Node &rhs) { return lhs < rhs.key; });
  --iter;

  auto lambda = [this, key](auto &child) -> DataType * {
    return FindInNode(child.get(), key);
  };

  return std::visit(lambda, iter->value);
}

template <typename T>
typename BTree<T>::DataType *BTree<T>::FindInNode(DataBlock *node,
                                                  const std::string &key) {
  auto iter = std::upper_bound(node->nodes.begin(), node->nodes.end(), key,
                               [](const std::string &lhs, const DataNode &rhs) {
                                 return lhs < rhs.key;
                               });
  --iter;

  if (iter->key == key) {
    return &(iter->value);
  } else {
    return nullptr;
  }
}

template <typename T>
template <class DType>
std::optional<std::vector<typename BTree<T>::template BaseNode<DType>>>
BTree<T>::InsertMaybeSplit(std::vector<BTree<T>::BaseNode<DType>> &vec,
                           const std::string &key, DType &&value) {
  auto iter =
      std::lower_bound(vec.begin(), vec.end(), key,
                       [](const BaseNode<DType> &lhs, const std::string &rhs) {
                         return lhs.key < rhs;
                       });

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

  --iter;

  BlockPointer new_child;
  std::string new_key;

  auto lambda = [this, key, value, &new_child, &new_key](auto &child) -> bool {
    using BlockType =
        typename std::remove_reference<decltype(*child.get())>::type;
    auto split = InsertInNode(child.get(), key, value);
    if (!split) {
      return false;
    }
    new_key = split.value()[0].key;
    auto new_child_typed = std::make_unique<BlockType>(
        BlockType{std::move(split.value()), child->next});
    child->next = new_child_typed.get();
    new_child = std::move(new_child_typed);
    return true;
  };

  if (std::visit(lambda, iter->value)) {
    return InsertMaybeSplit(node->nodes, new_key, std::move(new_child));
  } else {
    return {};
  }
}

template <typename T>
std::optional<std::vector<typename BTree<T>::DataNode>>
BTree<T>::InsertInNode(DataBlock *node, const std::string &key,
                       const T &value) {
  return InsertMaybeSplit(node->nodes, key, {value});
}

template <typename T>
void BTree<T>::PrintLeaves() {
  BlockPointer* node = &root_;
  while(!std::holds_alternative<std::unique_ptr<DataBlock>>(*node)) {
    node = &(std::get<std::unique_ptr<NodeBlock>>(*node)->nodes[0].value);
  }
  DataBlock* data_node = std::get<std::unique_ptr<DataBlock>>(*node).get();
  while(data_node != nullptr) {
    for (auto i : data_node->nodes) {
      if (i.value) {
        std::cout << i.value.value() << ", ";
      }
    }
    data_node = data_node->next;
  }
  std::cout << std::endl;
}
