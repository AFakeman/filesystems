#include "Ext2Driver.hpp"

#include <system_error>

#include <cstring>

#include <fcntl.h>
#include <unistd.h>

const int kBaseOffset = 1024;
const size_t kIndirectBlockPointer = 12;
const size_t kDoublyIndirectPointer = 13;
const size_t kTriplyIndirectPointer = 14;
const size_t kRootInode = 2;
const uint64_t kMaxFD = 2048;

enum class InodeType {
  FIFO = 0x1000,
  CharDevice = 0x2000,
  Directory = 0x4000,
  BlockDevice = 0x6000,
  File = 0x8000,
  Symlink = 0xA000,
  UnixSocket = 0xC000,
};

bool IsDirectory(const OpenFile &file) {
    return (file.inode.i_mode & static_cast<size_t>(InodeType::Directory)) ==
        static_cast<size_t>(InodeType::Directory);
}

bool Ext2Driver::IsDirectBlock(size_t file_block_idx) {
  return file_block_idx < DirectBlockPointers();
}

bool Ext2Driver::IsIndirectBlock(size_t file_block_idx) {
  return (DirectBlockPointers() <= file_block_idx) &&
         (file_block_idx < DirectBlockPointers() + IndirectBlockPointers());
}

bool Ext2Driver::IsDoublyIndirectBlock(size_t file_block_idx) {
  return (DirectBlockPointers() + IndirectBlockPointers() <= file_block_idx) &&
         (file_block_idx < DirectBlockPointers() + IndirectBlockPointers() +
                               DoublyIndirectBlockPointers());
}

bool Ext2Driver::IsTriplyIndirectBlock(size_t file_block_idx) {
  return (DirectBlockPointers() + IndirectBlockPointers() +
              DoublyIndirectBlockPointers() <=
          file_block_idx) &&
         (file_block_idx < DirectBlockPointers() + IndirectBlockPointers() +
                               DoublyIndirectBlockPointers() +
                               TriplyIndirectBlockPointers());
}

size_t Ext2Driver::DirectBlockPointers() const {
  return 12;
}

size_t Ext2Driver::IndirectBlockPointers() const {
  return block_size_ / sizeof(BlockIdxType);
}

size_t Ext2Driver::DoublyIndirectBlockPointers() const {
  return IndirectBlockPointers() * IndirectBlockPointers();
}

size_t Ext2Driver::TriplyIndirectBlockPointers() const {
  return IndirectBlockPointers() * DoublyIndirectBlockPointers();
}

size_t Ext2Driver::GetBlockOffset(size_t block_idx) const {
  return block_idx * block_size_;
}

size_t Ext2Driver::GetGroupOffset(size_t group_idx) const {
  return block_size_ * (group_idx * sb_.s_blocks_per_group + 1);
}


size_t Ext2Driver::IndirectBlockAddress(size_t file_block_idx) {
  if (!IsIndirectBlock(file_block_idx)) {
    return IndirectBlockPointers();
  }
  file_block_idx -= DirectBlockPointers();
  return file_block_idx;
}

std::array<size_t, 2> Ext2Driver::DoublyIndirectBlockAddress(size_t file_block_idx) {
  if (!IsDoublyIndirectBlock(file_block_idx)) {
    return {IndirectBlockPointers(), IndirectBlockPointers()};
  }
  file_block_idx -= DirectBlockPointers();
  file_block_idx -= IndirectBlockPointers();
  return {file_block_idx / IndirectBlockPointers(), file_block_idx % IndirectBlockPointers()};
}

std::array<size_t, 3> Ext2Driver::TriplyIndirectBlockAddress(size_t file_block_idx) {
  if (!IsTriplyIndirectBlock(file_block_idx)) {
    return {IndirectBlockPointers(), IndirectBlockPointers(), IndirectBlockPointers()};
  }
  file_block_idx -= DirectBlockPointers();
  file_block_idx -= IndirectBlockPointers();
  file_block_idx -= DoublyIndirectBlockPointers();
  size_t double_address = file_block_idx / DoublyIndirectBlockPointers();
  size_t single_address = (file_block_idx % DoublyIndirectBlockPointers()) / IndirectBlockPointers();
  size_t direct_address = file_block_idx % IndirectBlockPointers(); // One modulo is redundant
  return {double_address, single_address, direct_address};
}

Ext2Driver::Ext2Driver(const std::string &image) : image_(image) {}

Ext2Driver::~Ext2Driver() {
  if (fd_ > 0) {
    close(fd_);
  }
}

void Ext2Driver::Initialize() {
  fd_ = open(image_.c_str(), O_RDONLY);
  if (fd_ < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Could not open image");
  }
  if (lseek(fd_, kBaseOffset, SEEK_SET) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Error seeking a superblock");
  }
  if (read(fd_, &this->sb_, sizeof(this->sb_)) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Error reading first superblock in an image");
  }
  block_size_ = 1024 << sb_.s_log_block_size;
}

void Ext2Driver::Getattr(const char *path, struct stat *stat) {
  size_t inode_idx = GetInodeIdxByPath(path);
  ext2_inode inode;
  GetInodeByNumber(inode_idx, &inode);
  stat->st_mode = inode.i_mode;
  stat->st_nlink = inode.i_links_count;
  stat->st_uid = inode.i_uid;
  stat->st_gid = inode.i_gid;
  stat->st_size = inode.i_size;
  stat->st_atime = inode.i_atime;
  stat->st_ctime = inode.i_ctime;
  stat->st_mtime = inode.i_mtime;
  stat->st_blocks = inode.i_blocks;
}

int Ext2Driver::Readlink(const char *path, char *buf, size_t len) {
    size_t inode_idx = GetInodeIdxByPath(path);
    OpenFile file = OpenFileByInodeNumber(inode_idx);
    return ReadFile(file, buf, len, 0);
}

uint64_t Ext2Driver::Open(const char *path) {
  size_t inode_idx = GetInodeIdxByPath(path);
  for (int i = 0; i < kMaxFD; ++i) {
    if (open_files_.find(i) == open_files_.end()) {
      OpenFile new_file;
      new_file.inode_idx = inode_idx;
      GetInodeByNumber(inode_idx, &new_file.inode);
      open_files_.emplace(i, new_file);
      return i;
    }
  }
  throw std::system_error(ENFILE, std::generic_category());
}

int Ext2Driver::Read(uint64_t fd, char *buf, size_t len, off_t off) {
  auto it = open_files_.find(fd);
  if (it == open_files_.end()) {
    throw std::system_error(EBADF, std::generic_category());
  }
  return ReadFile(it->second, buf, len, off);
}

int Ext2Driver::ReadFile(OpenFile &file, char *buf, size_t len, off_t off) {
  if (off >= file.inode.i_size) {
    return 0;
  }
  if (off + len >= file.inode.i_size) {
    len = file.inode.i_size - off;
  }
  size_t block_start = off / block_size_;
  size_t block_end = (off + len - 1) / block_size_;
  size_t block_start_offset = off % block_size_;
  const char* src_buf = buf;
  ReadFileBlock(file, block_start);
  if (block_start == block_end) {
    memcpy(buf, file.FileData.data() + block_start_offset, len);
    return len;
  }
  size_t copy_length = block_size_ - block_start_offset;
  memcpy(buf, file.FileData.data() + block_start_offset, copy_length);
  buf += copy_length;
  len -= copy_length;
  for (size_t block = block_start + 1; block < block_end; ++block) {
    copy_length = block_size_;
    ReadFileBlock(file, block);
    memcpy(buf, file.FileData.data(), copy_length);
    buf += copy_length;
    len -= copy_length;
  }
  ReadFileBlock(file, block_end);
  memcpy(buf, file.FileData.data(), len);
  buf += len;
  len -= len;
  return buf - src_buf;
}

void Ext2Driver::Close(uint64_t fd) {
  auto it = open_files_.find(fd);
  if (it == open_files_.end()) {
    throw std::system_error(EBADF, std::generic_category());
  }
  open_files_.erase(it);
}

uint64_t Ext2Driver::Opendir(const char *path) {
  int fd = Open(path);
  if (!IsDirectory(open_files_[fd])) {
    throw std::system_error(ENOTDIR, std::generic_category());
  }
  return fd;
}

std::optional<std::string> Ext2Driver::Readdir(uint64_t fd) {
  auto it = open_files_.find(fd);
  if (it == open_files_.end()) {
    throw std::system_error(EINVAL, std::generic_category());
  }
  return ReaddirFile(it->second);
}

std::optional<std::string> Ext2Driver::ReaddirFile(OpenFile &file) {
  if (file.file_block_idx * block_size_ >= file.inode.i_size) {
    return {};
  }
  ReadFileBlock(file, file.file_block_idx);
  ext2_dir_entry_2 *direntry =
      reinterpret_cast<ext2_dir_entry_2 *>(file.FileData.data() + file.offset);
  file.offset += direntry->rec_len;
  if (file.offset > block_size_) {
    throw std::system_error(EIO, std::generic_category());
  } else if (file.offset == block_size_) {
    file.file_block_idx += 1;
  }
  char entry_filename[EXT2_NAME_LEN + 1];
  entry_filename[EXT2_NAME_LEN] = '\0';
  memcpy(entry_filename, direntry->name, direntry->name_len);
  entry_filename[direntry->name_len] = '\0';
  return {entry_filename};
}

void Ext2Driver::Releasedir(uint64_t fd) {
  Close(fd);
}

void Ext2Driver::GetInodeByNumber(size_t inode_idx, ext2_inode *buf) {
  inode_idx--;
  size_t group_number = inode_idx / sb_.s_inodes_per_group;
  size_t inode_idx_in_block = inode_idx % sb_.s_inodes_per_group;
  size_t group_offset = GetGroupOffset(group_number);
  size_t group_desc_offset = group_offset + block_size_;

  if (lseek(fd_, group_desc_offset, SEEK_SET) < 0) {
    char error_msg[1024];
    snprintf(error_msg, sizeof(error_msg), "Error seeking block group %lu",
             group_number);
    throw std::system_error(errno, std::generic_category(), error_msg);
  }

  struct ext2_group_desc gd {};
  if (read(fd_, &gd, sizeof(gd)) != sizeof(gd)) {
    char error_msg[1024];
    snprintf(error_msg, sizeof(error_msg),
             "Error reading group description of %lu", group_number);
    throw std::system_error(errno, std::generic_category(), error_msg);
  }

  size_t inode_bitmap_offset =
      GetBlockOffset(gd.bg_inode_bitmap) + inode_idx_in_block / 8;
  if (lseek(fd_, inode_bitmap_offset, SEEK_SET) < 0) {
    char error_msg[1024];
    snprintf(error_msg, sizeof(error_msg),
             "Error seeking inode bitmap for block %lu", group_number);
    throw std::system_error(errno, std::generic_category(), error_msg);
  }

  char bm;
  if (read(fd_, &bm, 1) != 1) {
    char error_msg[1024];
    snprintf(error_msg, sizeof(error_msg),
             "Error reading inode bitmap for inode %lu", inode_idx);
    throw std::system_error(errno, std::generic_category(), error_msg);
  }
  if (((bm >> (inode_idx_in_block % 8)) & 1) == 0) {
    char error_msg[1024];
    snprintf(error_msg, sizeof(error_msg), "Inode %lu is free", inode_idx);
    throw std::runtime_error(error_msg);
  }

  size_t inode_table_offset = gd.bg_inode_table * block_size_;

  size_t inode_offset =
      inode_table_offset + inode_idx_in_block * sizeof(ext2_inode);
  if (lseek(fd_, inode_offset, SEEK_SET) < 0) {
    char error_msg[1024];
    snprintf(error_msg, sizeof(error_msg), "Error seeking inode record for %lu",
             inode_idx);
    throw std::system_error(errno, std::generic_category(), error_msg);
  }

  if (read(fd_, buf, sizeof(*buf)) != sizeof(*buf)) {
    char error_msg[1024];
    snprintf(error_msg, sizeof(error_msg), "Failure reading inode %lu",
             inode_idx);
    throw std::system_error(errno, std::generic_category(), error_msg);
  }
}

void Ext2Driver::ReadFileBlock(OpenFile &file, size_t file_block_idx) {
  if (file.FileData.empty()) {
    file.file_block_idx = -1;
  }

  if (file_block_idx == file.file_block_idx) {
    return;
  }

  if (IsDirectBlock(file_block_idx)) {
    size_t block_idx = file.inode.i_block[file_block_idx];
    ReadBlock(block_idx, file.FileData);
    file.IndirectBlock.clear();
    file.DoublyIndirectBlock.clear();
    file.TriplyIndirectBlock.clear();
    file.file_block_idx = file_block_idx;
  }

  if (IsIndirectBlock(file_block_idx)) {
    if (!IsIndirectBlock(file.file_block_idx)) {
      ReadBlock(file.inode.i_block[kIndirectBlockPointer], file.IndirectBlock);
    }
    size_t block_addr = IndirectBlockAddress(file_block_idx);
    size_t block_idx = reinterpret_cast<BlockIdxType *>(file.IndirectBlock.data())[block_addr];
    ReadBlock(block_idx, file.FileData);
    file.DoublyIndirectBlock.clear();
    file.TriplyIndirectBlock.clear();
    file.file_block_idx = file_block_idx;
  }

  if (IsDoublyIndirectBlock(file_block_idx)) {
    if (!IsDoublyIndirectBlock(file.file_block_idx)) {
      ReadBlock(file.inode.i_block[kDoublyIndirectPointer],
                file.DoublyIndirectBlock);
    }
    std::array<size_t, 2> block_addr =
        DoublyIndirectBlockAddress(file_block_idx);
    std::array<size_t, 2> file_block_addr =
        DoublyIndirectBlockAddress(file.file_block_idx);

    bool equal = true;

    if (block_addr[0] != file_block_addr[0]) {
      size_t indirect_block_idx = reinterpret_cast<BlockIdxType *>(
          file.DoublyIndirectBlock.data())[block_addr[0]];
      ReadBlock(indirect_block_idx, file.IndirectBlock);
      equal = false;
    }

    size_t block_idx = reinterpret_cast<BlockIdxType *>(
        file.IndirectBlock.data())[block_addr[1]];
    ReadBlock(block_idx, file.FileData);
  }

  if (IsTriplyIndirectBlock(file_block_idx)) {
    if (!IsTriplyIndirectBlock(file.file_block_idx)) {
      ReadBlock(file.inode.i_block[kTriplyIndirectPointer],
                file.TriplyIndirectBlock);
    }
    std::array<size_t, 3> block_addr =
        TriplyIndirectBlockAddress(file_block_idx);
    std::array<size_t, 3> file_block_addr =
        TriplyIndirectBlockAddress(file.file_block_idx);

    bool equal = true;

    if (block_addr[0] != file_block_addr[0]) {
      size_t doubly_indirect_block_idx = reinterpret_cast<BlockIdxType *>(
          file.TriplyIndirectBlock.data())[block_addr[0]];
      ReadBlock(doubly_indirect_block_idx, file.DoublyIndirectBlock);
      equal = false;
    }

    if (!equal || block_addr[1] != file_block_addr[1]) {
      size_t indirect_block_idx = reinterpret_cast<BlockIdxType *>(
          file.DoublyIndirectBlock.data())[block_addr[1]];
      ReadBlock(indirect_block_idx, file.IndirectBlock);
      equal = false;
    }

    size_t block_idx = reinterpret_cast<BlockIdxType *>(
        file.DoublyIndirectBlock.data())[block_addr[2]];
    ReadBlock(block_idx, file.FileData);
  }

  file.file_block_idx = file_block_idx;
}

void Ext2Driver::ReadBlock(size_t block_idx, std::vector<char> &buf) {
  if (buf.size() != block_size_) {
    buf.resize(block_size_);
  }
  size_t offset = GetBlockOffset(block_idx);
  if (lseek(fd_, offset, SEEK_SET) < 0) {
    char error_msg[1024];
    snprintf(error_msg, sizeof(error_msg), "Couldn't seek block %lu",
             block_idx);
    throw std::system_error(errno, std::generic_category(), error_msg);
  }
  if (read(fd_, buf.data(), block_size_) != block_size_) {
    char error_msg[1024];
    snprintf(error_msg, sizeof(error_msg), "Couldn't read block %lu",
             block_idx);
    throw std::system_error(errno, std::generic_category(), error_msg);
  }
}

size_t Ext2Driver::GetInodeIdxByPath(const char *path) {
  if (path[0] != '/') {
    throw std::system_error(ENOENT, std::generic_category());
  }
  std::vector<std::string> tok_path = {""};
  for (const char *str = path; *str; str++) {
    if (*str != '/') {
      tok_path.back() += *str;
    } else if (*str == '/' && !tok_path.back().empty()) {
      tok_path.emplace_back("");
    }
  }
  if (tok_path.back().empty()) {
    tok_path.pop_back();
  }
  if (tok_path.empty()) {
    return kRootInode;
  }
  std::string destination = tok_path.back();
  tok_path.pop_back();
  size_t dir_inode = kRootInode;
  OpenFile dir = OpenFileByInodeNumber(dir_inode);
  for (auto i : tok_path) {
    if (!IsDirectory(dir)) {
      throw std::system_error(ENOTDIR, std::generic_category());
    }
    dir_inode = FindInDirectory(i.c_str(), dir);
    if (dir_inode == 0) {
      throw std::system_error(ENOENT, std::generic_category());
    }
    dir = OpenFileByInodeNumber(dir_inode);
  }
  size_t inode_idx = FindInDirectory(destination.c_str(), dir);
  if (inode_idx == 0) {
    throw std::system_error(ENOENT, std::generic_category());
  }
  return inode_idx;
}

OpenFile Ext2Driver::OpenFileByInodeNumber(size_t inode_idx) {
  OpenFile file;
  file.inode_idx = inode_idx;
  GetInodeByNumber(file.inode_idx, &file.inode);
  return file;
}

size_t Ext2Driver::FindInDirectory(const char *filename, OpenFile &directory) {
  if (!IsDirectory(directory)) {
    throw std::system_error(ENOTDIR, std::generic_category());
  }
  size_t block = 0;
  size_t bytes_read = 0;
  char entry_filename[EXT2_NAME_LEN + 1];
  while (bytes_read < directory.inode.i_size) {
    ReadFileBlock(directory, block);
    char *data = reinterpret_cast<char*>(directory.FileData.data());
    for (size_t index = 0; index < block_size_;) {
      ext2_dir_entry_2 *dirent = reinterpret_cast<ext2_dir_entry_2*>(data + index);
      index += dirent->rec_len;
      std::memcpy(entry_filename, dirent->name, dirent->name_len);      
      entry_filename[dirent->name_len] = '\0';
      if (std::strcmp(filename, entry_filename) == 0) {
        return dirent->inode;
      }
    }
    bytes_read += block_size_;
    block++;
  }
  return 0;
}
