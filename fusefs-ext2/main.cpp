#define FUSE_USE_VERSION 31

#include <cstdint>
#include <cstring>
#include <string>
#include <system_error>

#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stdio.h>
#include <unistd.h>

#include "Ext2Driver.hpp"

/**
 * struct ext2_super_block
 * struct ext2_group_desc
 * struct ext2_acl_entry
 * struct ext2_acl_header
 * struct ext2_inode
 */

Ext2Driver *private_data() {
  return static_cast<Ext2Driver *>(fuse_get_context()->private_data);
}

int myfs_getattr(const char *path, struct stat *stbuf) {
  Ext2Driver *cast = private_data();
  try {
    cast->Getattr(path, stbuf);
  } catch (const std::system_error &err) {
    return -err.code().value();
  }
  return 0;
}

int myfs_readlink(const char *path, char *buf, size_t len) {
  Ext2Driver *cast = private_data();
  try {
    return cast->Readlink(path, buf, len);
  } catch (const std::system_error &err) {
    return -err.code().value();
  }
}

int myfs_open(const char *path, struct fuse_file_info *info) {
  Ext2Driver *cast = private_data();
  try {
    info->fh = cast->Open(path);
  } catch (const std::system_error &err) {
    return -err.code().value();
  }
  return 0;
}

int myfs_read(const char *path, char *buf, size_t len, off_t off,
              struct fuse_file_info *info) {
  Ext2Driver *cast = private_data();
  try {
    return cast->Read(info->fh, buf, len, off);
  } catch (const std::system_error &err) {
    return -err.code().value();
  }
}

int myfs_release(const char *path, struct fuse_file_info *info) {
  Ext2Driver *cast = private_data();
  try {
    cast->Close(info->fh);
  } catch (const std::system_error &err) {
    return -err.code().value();
  }
  return 0;
}

int myfs_opendir(const char *path, struct fuse_file_info *info) {
  Ext2Driver *cast = private_data();
  try {
    info->fh = cast->Opendir(path);
  } catch (const std::system_error &err) {
    return -err.code().value();
  }
  return 0;
}

int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t off,
                 struct fuse_file_info *info) {
  Ext2Driver *cast = private_data();
  try {
    auto name = cast->Readdir(info->fh);
    while (name.has_value()) {
      filler(buf, name.value_or("").c_str(), NULL, 0);
      name = cast->Readdir(info->fh);
    }
  } catch (const std::system_error &err) {
    return -err.code().value();
  }
  return 0;
}

int myfs_releasedir(const char *path, struct fuse_file_info *info) {
  Ext2Driver *cast = private_data();
  try {
    cast->Releasedir(info->fh);
  } catch (const std::system_error &err) {
    return -err.code().value();
  }
  return 0;
}

void myfs_destroy(void *private_data) {
  Ext2Driver *cast = static_cast<Ext2Driver*>(private_data);
  delete cast;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: ext2fuse <image> [fuse_args...]");
    return 2;
  }

  struct fuse_operations myfs_oper = {};
  myfs_oper.getattr = myfs_getattr;
  myfs_oper.readlink = myfs_readlink;
  myfs_oper.open = myfs_open;
  myfs_oper.read = myfs_read;
  myfs_oper.release = myfs_release;
  myfs_oper.opendir = myfs_opendir;
  myfs_oper.readdir = myfs_readdir;
  myfs_oper.releasedir = myfs_releasedir;
  myfs_oper.destroy = myfs_destroy;

  Ext2Driver *private_data = new Ext2Driver(argv[1]);
  private_data->Initialize();
  fprintf(stderr, "about to call fuse_main\n");
  argv[1] = argv[0];
  int fuse_stat = fuse_main(argc - 1, argv + 1, &myfs_oper, private_data);
  fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
}
