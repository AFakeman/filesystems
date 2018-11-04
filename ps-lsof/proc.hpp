#pragma once

#include <string>
#include <vector>

std::vector<std::string> get_pids();

std::string get_cmdline(const std::string &pid);

std::string get_username(const std::string &pid);

std::string get_time(const std::string &pid);

std::vector<std::string> get_open_files(const std::string &pid);
