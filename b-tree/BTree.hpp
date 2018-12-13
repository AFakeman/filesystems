#include <algorithm>
#include <exception>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

template <class T, size_t block_size = 16> class BTree {
public:
  BTree();
  void Insert(const std::string &key, const T &value);
  T &Get(const std::string &key);
  void Pop(const std::string &key);
  bool Contains(const std::string &key);

  size_t size() const {return size_;};

  void PrintLeaves();

  static BTree Merge(BTree &lhs, BTree &rhs);

  template <class DataType> struct BaseNode {
    std::string key;
    DataType value;
    bool operator==(const BaseNode &rhs) {
      return key == rhs.key;
    }

    bool operator<(const BaseNode &rhs) {
      return key < rhs.key;
    }

    bool operator>(const BaseNode &rhs) {
      return key > rhs.key;
    }
  };

  template <class DataType> struct BaseBlock {
    using NodeType = BaseNode<DataType>;
    std::vector<NodeType> nodes;
    BaseBlock *next{nullptr};
  };

  using DataType = std::optional<T>;
  using DataBlock = BaseBlock<DataType>;
  using DataNode = BaseNode<DataType>;

  struct NodeBlock;
  using BlockPointer =
      std::variant<std::unique_ptr<DataBlock>, std::unique_ptr<NodeBlock>>;
  struct NodeBlock : public BaseBlock<BlockPointer> {};
  using Node = BaseNode<BlockPointer>;

  class Iterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = DataNode;
    using difference_type = int;
    using pointer = value_type*;
    using reference = value_type&;

    Iterator() = default;
    Iterator(const Iterator &) = default;

    reference operator*() { return *it_; }

    pointer operator->() { return &(*it_); }

    bool operator==(const Iterator &rhs) const {
      return (bl_ == rhs.bl_) && ((it_ == rhs.it_) || (bl_ == nullptr));
    }

    bool operator!=(const Iterator &rhs) const {
      return !(*this == rhs);
    }

    Iterator &operator++() {
      Increment();
      return *this;
    }

    Iterator operator++(int) {
      Iterator copy = *this;
      ++(*this);
      return copy;
    }

  private:
    friend class BTree;
    using vector_iterator = typename std::vector<DataNode>::iterator;

    void Increment() {
      if (bl_ == nullptr) {
        return;
      }

      do {
        ++it_;

        if (it_ == bl_->nodes.end()) {
          bl_ = bl_->next;
          if (bl_ != nullptr) {
            it_ = bl_->nodes.begin();
          }
        }
      } while (bl_ != nullptr && !it_->value);
    }

    Iterator(DataBlock *bl, const vector_iterator &it) : bl_(bl), it_(it) {
      Increment();
    }

    Iterator(DataBlock *bl) : bl_(bl) {
      if (bl_ != nullptr) {
        it_ = bl_->nodes.begin();
      }
      Increment();
    }

    DataBlock *bl_;
    vector_iterator it_;
  };

  Iterator begin() {return Iterator(GetLeftLeaf().get());}
  Iterator end() {return Iterator(nullptr);}
private:
  BTree(BlockPointer &&root, size_t size) : root_(std::move(root)), size_(size) {}

  std::optional<std::vector<Node>>
  InsertInNode(NodeBlock *node, const std::string &key, const T &value);

  std::optional<std::vector<DataNode>>
  InsertInNode(DataBlock *node, const std::string &key, const T &value);

  template <class DType>
  std::optional<std::vector<BaseNode<DType>>>
  InsertMaybeSplit(std::vector<BTree::BaseNode<DType>> &vec,
                   const std::string &key, DType &&value);

  DataType *Find(const std::string &key);
  DataType *FindInNode(NodeBlock *node, const std::string &key);
  DataType *FindInNode(DataBlock *node, const std::string &key);

  static std::vector<std::unique_ptr<DataBlock>> GenerateLeafLevel(BTree &lhs,
                                                            BTree &rhs);

  std::unique_ptr<DataBlock> &GetLeftLeaf();

  size_t size_{0};
  BlockPointer root_;
};

template <typename T, size_t block_size>
BTree<T, block_size>::BTree()
    : root_(std::make_unique<DataBlock>(DataBlock{{{"", {}}}, nullptr})) {}

template <typename T, size_t block_size>
void BTree<T, block_size>::Insert(const std::string &key, const T &value) {
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

template <typename T, size_t block_size> bool BTree<T, block_size>::Contains(const std::string &key) {
  DataType *result = Find(key);
  return result && *result;
}

template <typename T, size_t block_size> void BTree<T, block_size>::Pop(const std::string &key) {
  DataType *item = Find(key);
  if (item) {
    item->reset();
    size_--;
  }
}

template <typename T, size_t block_size> T &BTree<T, block_size>::Get(const std::string &key) {
  DataType *item = Find(key);
  if (item) {
    return item->value();
  } else {
    throw std::runtime_error("No such element");
  }
}

template <typename T, size_t block_size>
typename BTree<T, block_size>::DataType *BTree<T, block_size>::Find(const std::string &key) {
  auto lambda = [this, key](auto &root) -> DataType * {
    return FindInNode(root.get(), key);
  };

  return std::visit(lambda, root_);
}

template <typename T, size_t block_size>
typename BTree<T, block_size>::DataType *BTree<T, block_size>::FindInNode(NodeBlock *node,
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

template <typename T, size_t block_size>
typename BTree<T, block_size>::DataType *BTree<T, block_size>::FindInNode(DataBlock *node,
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

template <typename T, size_t block_size>
template <class DType>
std::optional<std::vector<typename BTree<T, block_size>::template BaseNode<DType>>>
BTree<T, block_size>::InsertMaybeSplit(std::vector<BTree<T, block_size>::BaseNode<DType>> &vec,
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

  std::vector<BTree<T, block_size>::BaseNode<DType>> rest;
  rest.insert(rest.end(), std::make_move_iterator(vec.begin() + vec.size() / 2),
              std::make_move_iterator(vec.end()));
  vec.resize(vec.size() / 2);
  return rest;
}

template <typename T, size_t block_size>
std::optional<std::vector<typename BTree<T, block_size>::Node>>
BTree<T, block_size>::InsertInNode(NodeBlock *node, const std::string &key,
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

template <typename T, size_t block_size>
std::optional<std::vector<typename BTree<T, block_size>::DataNode>>
BTree<T, block_size>::InsertInNode(DataBlock *node, const std::string &key,
                       const T &value) {
  size_++;
  return InsertMaybeSplit(node->nodes, key, {value});
}

template <typename T, size_t block_size>
void BTree<T, block_size>::PrintLeaves() {
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

template <typename T, size_t block_size>
std::unique_ptr<typename BTree<T, block_size>::DataBlock> &BTree<T, block_size>::GetLeftLeaf() {
  BlockPointer* node = &root_;
  while(!std::holds_alternative<std::unique_ptr<DataBlock>>(*node)) {
    node = &(std::get<std::unique_ptr<NodeBlock>>(*node)->nodes[0].value);
  }
  return std::get<std::unique_ptr<DataBlock>>(*node);
}

template <typename T, size_t block_size>
std::vector<std::unique_ptr<typename BTree<T, block_size>::DataBlock>>
BTree<T, block_size>::GenerateLeafLevel(BTree &lhs, BTree &rhs) {
  std::vector<std::unique_ptr<DataBlock>> result;
  result.emplace_back(std::make_unique<DataBlock>());
  auto lhs_it = lhs.begin();
  auto rhs_it = rhs.begin();
  while (lhs_it != lhs.end() || rhs_it != rhs.end()) {
    if (*lhs_it == *rhs_it) {
      std::string err_msg = "Duplicate key: ";
      err_msg += lhs_it->key; // Sum so the key doesn't get cut.
      throw std::runtime_error(err_msg.c_str());
    }
    if (result.back()->nodes.size() == block_size) {
      result.push_back(std::make_unique<DataBlock>(DataBlock{{}, nullptr}));
      result[result.size() - 2]->next = result.back().get();
    }
    if ((lhs_it != lhs.end()) &&
        ((rhs_it == rhs.end()) || (*lhs_it < *rhs_it))) {
      result.back()->nodes.push_back(*lhs_it++);
    } else {
      result.back()->nodes.push_back(*rhs_it++);
    }
  }
  return result;
}

template <typename T, size_t block_size>
BTree<T, block_size> BTree<T, block_size>::Merge(BTree &lhs, BTree &rhs) {
  auto build_level = [] (auto &vec) -> std::vector<std::unique_ptr<NodeBlock>> {
    std::vector<std::unique_ptr<NodeBlock>> result;
    result.emplace_back(std::make_unique<NodeBlock>());
    for (auto &ptr : vec) {
      if (result.back()->nodes.size() == block_size) {
        result.push_back(std::make_unique<NodeBlock>());
        result[result.size() - 2]->next = result.back().get();
      }
      std::string key = ptr->nodes[0].key;
      result.back()->nodes.emplace_back(Node{key, std::move(ptr)});
    }
    return result;
  };
  auto data = GenerateLeafLevel(lhs, rhs);
  size_t size = 0;
  for (auto &ptr : data) {
    size += ptr->nodes.size();
  }
  if (data.size() == 1) {
    return BTree(std::move(data.front()), size);
  }
  auto nodes = build_level(data);
  while (nodes.size() != 1) {
    nodes = build_level(nodes);
  }
  return BTree(std::move(nodes.front()), size);
}

