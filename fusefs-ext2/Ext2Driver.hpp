#include <array>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

#include <ext2fs/ext2_fs.h>
#include <sys/stat.h>

struct OpenFile {
  size_t offset{0};
  size_t file_block_idx{0};
  size_t inode_idx{0};
  ext2_inode inode;
  std::vector<char> FileData{};
  std::vector<char> IndirectBlock{};
  std::vector<char> DoublyIndirectBlock{};
  std::vector<char> TriplyIndirectBlock{};
};

class Ext2Driver {
public:
  Ext2Driver(const std::string &image);

  ~Ext2Driver();

  void Initialize();

  void Getattr(const char *path, struct stat *stat);
  uint64_t Open(const char *path);
  int Read(uint64_t fd, char *buf, size_t len, off_t off);
  void Close(uint64_t fd);
  uint64_t Opendir(const char *path);
  std::optional<std::string> Readdir(uint64_t fd);
  int Readlink(const char *path, char *buf, size_t len);
  void Releasedir(uint64_t fd);

private:
  typedef __u32 BlockIdxType;

  size_t GetBlockOffset(size_t block_idx) const;
  size_t GetGroupOffset(size_t group_idx) const;

  /**
   * These functions return how much pointers to the given block family is
   * there.
   */
  size_t DirectBlockPointers() const;
  size_t IndirectBlockPointers() const;
  size_t DoublyIndirectBlockPointers() const;
  size_t TriplyIndirectBlockPointers() const;

  bool IsDirectBlock(size_t file_block_idx);
  bool IsIndirectBlock(size_t file_block_idx);
  bool IsDoublyIndirectBlock(size_t file_block_idx);
  bool IsTriplyIndirectBlock(size_t file_block_idx);

  int ReadFile(OpenFile &file, char *buf, size_t len, off_t off);
  std::optional<std::string> ReaddirFile(OpenFile &file);
  /**
   * These functions generate the path to the block. If the file_block_idx
   * is not actually of the required redirection level, an impossible value
   * (IndirectBlockPointers()) is returned.
   */
  size_t IndirectBlockAddress(size_t file_block_idx);
  std::array<size_t, 2> DoublyIndirectBlockAddress(size_t file_block_idx);
  std::array<size_t, 3> TriplyIndirectBlockAddress(size_t file_block_idx);

  size_t GetInodeIdxByPath(const char *path);
  void GetInodeByNumber(size_t inode_idx, ext2_inode *buf);
  OpenFile OpenFileByInodeNumber(size_t inode_idx);
  void ReadFileBlock(OpenFile &file, size_t file_block_idx);
  void ReadBlock(size_t file_block_idx, std::vector<char> &buf);

  // Finds inode corresponding to a filename in a given directory.
  size_t FindInDirectory(const char *filename, OpenFile &directory);

  std::string image_;
  int fd_{};
  ext2_super_block sb_{};
  int block_size_;
  std::unordered_map<uint64_t, OpenFile> open_files_;
};
