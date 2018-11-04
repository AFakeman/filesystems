#include <iostream>

#include "proc.hpp"

int main() {
  std::cout << "PID" << '\t' << "CMD" << '\t' << "PATH" << std::endl;
  for (auto pid : get_pids()) {
    for (auto open_file : get_open_files(pid)) {
      std::cout << pid << '\t' << get_cmdline(pid) << '\t' << open_file << std::endl;
    }
  }
}
