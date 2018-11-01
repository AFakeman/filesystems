#include "converter.hpp"

namespace Converter {
const uint8_t kMarkers[] = {
    0b00000000,
    0b11000000,
    0b11100000,
    0b11110000,
};

const uint8_t kMasks[]{
    0b10000000,
    0b11100000,
    0b11110000,
    0b11111000,
};

const uint32_t kMaxCodePoints[] = {
    0x007F,
    0x07FF,
    0xFFFF,
    0x10FFFF,
};

const uint8_t kFirstByteCapacities[] = {
    7,
    5,
    4,
    3,
};

const uint8_t kCapacities[] = {
    7,
    11,
    16,
    21,
};

const uint8_t kResidualByteMarker = 0b10000000;
const uint8_t kResidualByteMask = 0b11000000;
const uint8_t kResidualByteCapacity = 6;

std::vector<uint32_t> utf8_to_utf32(const std::vector<uint8_t> &str) {
  std::vector<uint32_t> result;
  size_t idx = 0;
  while (idx < str.size()) { // Loop that extracts one symbol per iter
    uint32_t wide_symbol;
    uint8_t length = 0;

    for (uint8_t bytes = 0; bytes < sizeof(kMasks) / sizeof(kMasks[0]);
         ++bytes) {
      if ((str[idx] & kMasks[bytes]) == kMarkers[bytes]) {
        length = bytes + 1;
        wide_symbol = str[idx] & ~kMasks[bytes];
        break;
      }
    }

    if (length == 0) {
      throw std::runtime_error("Unknown starting byte");
    }
    if (idx + length > str.size()) {
      throw std::runtime_error("String too short");
    }

    for (uint8_t byte = 1; byte < length; ++byte) {
      if ((str[idx + byte] & kResidualByteMask) != kResidualByteMarker) {
        throw std::runtime_error("Invalid residual byte");
      }
      wide_symbol = wide_symbol << kResidualByteCapacity;
      wide_symbol |= str[idx + byte] & ~kResidualByteMask;
    }

    result.push_back(wide_symbol);
    idx += length;
  }
  return result;
}

/*
 * Returns a bit sequence in range [from, to].
 */
uint32_t substr(uint32_t str, uint8_t from, uint8_t to) {
  return ((str << (31 - to)) >> (31 - to)) >> from;
}

std::vector<uint8_t> utf32_to_utf8(const std::vector<uint32_t> &str) {
  std::vector<uint8_t> result;
  for (size_t idx = 0; idx < str.size(); ++idx) {
    uint32_t wide_symbol = str[idx];
    uint8_t length = 0;
    uint8_t remaining_bits;

    for (uint8_t bytes = 0;
         bytes < sizeof(kCapacities) / sizeof(kCapacities[0]); ++bytes) {
      if (wide_symbol <= kMaxCodePoints[bytes]) {
        uint8_t byte = kMarkers[bytes];
        length = bytes + 1;
        remaining_bits = kCapacities[bytes] - kFirstByteCapacities[bytes];
        byte |= (uint8_t)substr(wide_symbol, remaining_bits,
                                kCapacities[bytes] - 1);
        result.push_back(byte);
        break;
      }
    }

    if (length == 0) {
      throw std::runtime_error("Unsupported symbol");
    }

    for (size_t i = 1; i < length; ++i) {
      uint8_t byte = kResidualByteMarker;
      uint8_t to = remaining_bits - 1;
      remaining_bits -= kResidualByteCapacity;
      byte |= (uint8_t)substr(wide_symbol, remaining_bits, to);
      result.push_back(byte);
    }
  }
  return result;
}
} // namespace Converter
