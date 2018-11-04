#include <iostream>

#include "proc.hpp"

int main() {
  std::cout << "PID\tUSER\tTIME\tCOMMAND" << std::endl;
  for (auto pid : get_pids()) {
    std::cout << pid << '\t' << get_username(pid) << '\t' << get_time(pid) << '\t' << get_cmdline(pid) << std::endl;
  }
}
