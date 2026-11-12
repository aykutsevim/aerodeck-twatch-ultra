// AeroDeck core: the recorder's persistence record (S6 data-model §5). Pure, host-tested.
// [0] version 1, [1] record count 8, then today + 7 history records (12 bytes each, little-endian),
// then CRC-16/CCITT-FALSE over everything before it. 100 bytes.
#pragma once
#include <cstddef>
#include <cstdint>
#include "core/DayLog.h"

namespace core {
namespace RecBlob {

constexpr uint8_t kVersion = 1;
constexpr size_t kSize = 2 + 8 * 12 + 2;

size_t encode(const DayLog& log, uint8_t* out);                // writes kSize bytes
bool decode(const uint8_t* buf, size_t len, DayLog& out);      // false (out untouched) on length/version/CRC
uint16_t crc16(const uint8_t* p, size_t n);

} // namespace RecBlob
} // namespace core
