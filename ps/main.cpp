#include <dirent.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <pwd.h>

// There is no reliable way to get this kernel constant without some weird
// trickery. Who even uses non-x86 processors?
const size_t kHZ = 100;
const std::string kProcDir = "/proc/";

std::vector<std::string> get_pids() {
  DIR *dir = opendir(kProcDir.c_str());
  std::vector<std::string> result;
  struct dirent *dir_entry = readdir(dir);
  while (dir_entry != NULL) {
    bool digit_filename = true;
    for (size_t i = 0; dir_entry->d_name[i] != 0; ++i) {
      if (dir_entry->d_name[i] < '0' || dir_entry->d_name[i] > '9') {
        digit_filename = false;
      }
    }
    std::string full_name = std::string(dir_entry->d_name);
    if (digit_filename) {
      result.push_back(full_name);
    }
    dir_entry = readdir(dir);
  }
  return result;
}

// We accept pids as string to save on integer conversions, and
// as the program is not production-grade we will not verify pid.
std::string get_cmdline(const std::string &pid) {
  std::fstream proc_cmdline;
  std::string cmdline;
  proc_cmdline.open(kProcDir + pid + "/cmdline", std::fstream::in);
  // We may have newlines in the command, so let's read the file this way
  while(!proc_cmdline.eof()) {
    std::string line;
    std::getline(proc_cmdline, line);
    cmdline += line;
  }
  return cmdline;
}

std::string get_uid(const std::string &pid) {
  std::fstream proc_stat;
  proc_stat.open(kProcDir + pid + "/status", std::fstream::in);
  std::string uid_line;
  while(!proc_stat.eof()) {
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
    throw std::system_error(errno, std::generic_category());
  }
  return {pw->pw_name};
}

std::string get_username(const std::string &pid) {
  return uid_to_name(get_uid(pid));
}

unsigned long long get_time_jiffies(const std::string &pid) {
  std::fstream proc_stat;
  proc_stat.open(kProcDir + pid + "/stat");
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

int main() {
  auto pids = get_pids();
  std::cout << "PID\tUSER\tTIME\tCOMMAND" << std::endl;
  for (auto pid : pids) {
    std::cout << pid << '\t' << get_username(pid) << '\t' << get_time(pid) << '\t' << get_cmdline(pid) << std::endl;
  }
}
