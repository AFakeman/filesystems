#include <cassert>
#include <iostream>
#include <vector>

#include "converter.hpp"

const std::vector<uint32_t> correct_utf32_str = {
    0x42f, 0x20,  0x43b, 0x44e, 0x431, 0x43b, 0x44e,
    0x20,  0x43a, 0x443, 0x440, 0x438, 0x446};

const std::vector<uint8_t> correct_utf8_str = {
    0b11010000, 0b10101111, 0b100000,   0b11010000, 0b10111011, 0b11010001,
    0b10001110, 0b11010000, 0b10110001, 0b11010000, 0b10111011, 0b11010001,
    0b10001110, 0b100000,   0b11010000, 0b10111010, 0b11010001, 0b10000011,
    0b11010001, 0b10000000, 0b11010000, 0b10111000, 0b11010001, 0b10000110};

void test_correct_8_32_conversion() {
  const std::vector<uint32_t> test_utf32_str =
      Converter::utf8_to_utf32(correct_utf8_str);
  assert(test_utf32_str == correct_utf32_str);
}

void test_correct_32_8_conversion() {
  const std::vector<uint8_t> test_utf8_str =
      Converter::utf32_to_utf8(correct_utf32_str);
  assert(test_utf8_str == correct_utf8_str);
}

void test_incorrect_start_symbol() {
  bool thrown = false;
  std::vector<uint8_t> utf8_str = {0b11111000};
  try {
    std::vector<uint32_t> utf32_str = Converter::utf8_to_utf32(utf8_str);
  } catch (const std::runtime_error &e) {
    assert(e.what() == std::string("Unknown starting byte"));
    thrown = true;
  }
}

void test_string_too_short() {
  bool thrown = false;
  std::vector<uint8_t> utf8_str = {0b11110000};
  try {
    std::vector<uint32_t> utf32_str = Converter::utf8_to_utf32(utf8_str);
  } catch (const std::runtime_error &e) {
    assert(e.what() == std::string("String too short"));
    thrown = true;
  }
  assert(thrown == true);
}

void test_invalid_residual_byte() {
  bool thrown = false;
  std::vector<uint8_t> utf8_str = {0b11000000, 0b00000000};
  try {
    std::vector<uint32_t> utf32_str = Converter::utf8_to_utf32(utf8_str);
  } catch (const std::runtime_error &e) {
    assert(e.what() == std::string("Invalid residual byte"));
    thrown = true;
  }
  assert(thrown == true);
}

void test_unsupported_symbol() {
  bool thrown = false;
  std::vector<uint32_t> utf32_str = {0xFFFFFF};
  try {
    std::vector<uint8_t> utf8_str = Converter::utf32_to_utf8(utf32_str);
  } catch (const std::runtime_error &e) {
    assert(e.what() == std::string("Unsupported symbol"));
    thrown = true;
  }
  assert(thrown == true);
}

int main() {
  test_correct_8_32_conversion();
  test_correct_32_8_conversion();
  test_incorrect_start_symbol();
  test_string_too_short();
  test_invalid_residual_byte();
  test_unsupported_symbol();
  return 0;
}
