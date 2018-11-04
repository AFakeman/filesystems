#include "proc.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <dirent.h>
#include <pwd.h>
#include <unistd.h>

#include "string_magic.hpp"

// There is no reliable way to get this kernel constant without some weird
// trickery. Who even uses non-x86 processors?
const size_t kHZ = 100;
const std::string kProcDir = "/proc/";

/*
 * Returns all files in a directory.
 */
std::vector<std::string> ls(const std::string &dirname) {
  DIR *dir = opendir(dirname.c_str());
  if (dir == NULL) {
    throw std::system_error(errno, std::generic_category(), "Error on opendir");
  }
  std::vector<std::string> result;
  struct dirent *dir_entry = readdir(dir);
  while (dir_entry != NULL) {
    std::string full_name = std::string(dir_entry->d_name);
    if (dir_entry->d_name[0] != '.') {
      result.push_back(dir_entry->d_name);
    }
    dir_entry = readdir(dir);
  }
  closedir(dir);
  return result;
}

std::vector<std::string> get_pids() {
  std::vector<std::string> filenames = ls(kProcDir);
  auto nondigit_filename = [](const std::string &filename) {
    for (auto ch : filename) {
      if (ch > '9' || ch < '0')
        return true;
    }
    return false;
  };
  filenames.erase(
      std::remove_if(filenames.begin(), filenames.end(), nondigit_filename),
      filenames.end());
  return filenames;
}

// We accept pids as string to save on integer conversions, and
// as the program is not production-grade we will not verify pid.
std::string get_cmdline(const std::string &pid) {
  std::fstream proc_cmdline;
  std::string cmdline;
  std::string filename = build_string(kProcDir, pid, "/cmdline");
  proc_cmdline.open(filename);
  // We may have newlines in the command, so let's read the file this way
  while (!proc_cmdline.eof()) {
    std::string line;
    std::getline(proc_cmdline, line);
    cmdline += line;
  }
  return cmdline;
}

std::string get_uid(const std::string &pid) {
  std::fstream proc_stat;
  std::string filename = build_string(kProcDir, "status", filename);
  proc_stat.open(filename);
  std::string uid_line;
  while (!proc_stat.eof()) {
    getline(proc_stat, uid_line);
    if (uid_line.find("Uid:") == 0) {
      break;
    }
  }
  if (proc_stat.eof()) {
    throw std::runtime_error("UID is not provided in /proc/<pid>/status");
  }
  // We use the real uid
  size_t first_delim = uid_line.find("\t");
  size_t second_delim = uid_line.find("\t", first_delim + 1);
  size_t substr_len = second_delim - first_delim - 1;
  return uid_line.substr(first_delim + 1, substr_len);
}

std::string uid_to_name(const std::string &uid) {
  struct passwd *pw = getpwuid(std::stoi(uid));
  if (pw == NULL) {
    throw std::system_error(errno, std::generic_category(),
                            "Error on getpwuid");
  }
  return {pw->pw_name};
}

std::string get_username(const std::string &pid) {
  return uid_to_name(get_uid(pid));
}

unsigned long long get_time_jiffies(const std::string &pid) {
  std::fstream proc_stat;
  std::string filename = build_string(kProcDir, pid, "/stat");
  proc_stat.open(filename);
  // Needed stats are at position 14 and 15.
  for (size_t i = 0; i < 13; ++i) {
    std::string tmp;
    std::getline(proc_stat, tmp, ' ');
  }
  std::string utime;
  std::string stime;
  std::getline(proc_stat, utime, ' ');
  std::getline(proc_stat, stime, ' ');
  unsigned long long jiffies = std::stoull(utime) + std::stoull(stime);
  return jiffies;
}

std::string get_time(const std::string &pid) {
  unsigned long long jiffies = get_time_jiffies(pid);
  unsigned long long seconds = jiffies / kHZ;
  return std::to_string(seconds / 60) + ':' + std::to_string(seconds % 60);
}

std::vector<std::string> get_open_files(const std::string &pid) {
  std::string fd_dir = build_string(kProcDir, pid, "/fd/");
  std::vector<std::string> fds = ls(fd_dir);
  std::vector<std::string> result;
  char link_dest[1024];
  char filename[1024];
  // All symlinks start with /proc/<pid>/fd/
  strncpy(filename, fd_dir.c_str(), sizeof(filename));
  for (auto fd : fds) {
    // 1 for null, and don't forget the starting part
    strncat(filename, fd.c_str(), sizeof(filename) - 1 - fd_dir.size());
    int read_len = readlink(filename, link_dest, sizeof(link_dest));
    if (read_len != -1) {
      link_dest[read_len] = 0;
      result.push_back(link_dest);
    }
    // Let's reset the string back to /proc/<pid>/fd/
    filename[fd_dir.size()] = 0;
  }
  return result;
}
