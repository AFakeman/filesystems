#pragma once

#include <cstddef>
#include <iostream>
#include <cstdlib>
#include <string>

size_t str_length(const std::string &str) {
  return str.size();
}

size_t str_length(const char *str) {
  return strlen(str);
}

size_t variadic_strlen() {
  return 0;
}

template <class T, class ...Rest>
size_t variadic_strlen(T str, Rest... rest) {
  return str_length(str) + variadic_strlen(rest...);
}

void recursive_concat(std::string &result) {}

template <class T, class ...Rest>
void recursive_concat(std::string &result, const T &str, Rest... rest) {
  result += str;
  recursive_concat(result, rest...);
}

template <class ...Rest>
std::string build_string(Rest... rest) {
  std::string result;
  result.reserve(variadic_strlen(rest...));
  recursive_concat(result, rest...);
  return result;
}
