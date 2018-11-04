#include <string>
#include <vector>

std::vector<std::string> get_pids();

std::string get_cmdline(const std::string &pid);

std::string get_uid(const std::string &pid);

std::string uid_to_name(const std::string &uid);

std::string get_username(const std::string &pid);

unsigned long long get_time_jiffies(const std::string &pid);

std::string get_time(const std::string &pid);
