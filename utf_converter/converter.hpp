#include <cstddef>
#include <vector>

namespace Converter {
std::vector<uint32_t> utf8_to_utf32(const std::vector<uint8_t> &str);
std::vector<uint8_t> utf32_to_utf8(const std::vector<uint32_t> &str);
extern const uint8_t kMarkers[];
extern const uint8_t kMasks[];
extern const uint32_t kMaxCodePoints[];
extern const uint8_t kFirstByteCapacities[];
extern const uint8_t kCapacities[];
extern const uint8_t kResidualByteMarker;
extern const uint8_t kResidualByteMask;
extern const uint8_t kResidualByteCapacity;
} // namespace Converter
