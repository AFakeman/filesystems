#include <iostream>

#include "proc.hpp"

int main() {
  auto pids = get_pids();
  std::cout << "PID\tUSER\tTIME\tCOMMAND" << std::endl;
  for (auto pid : pids) {
    std::cout << pid << '\t' << get_username(pid) << '\t' << get_time(pid) << '\t' << get_cmdline(pid) << std::endl;
  }
}
