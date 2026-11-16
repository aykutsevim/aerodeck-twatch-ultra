// AeroDeck core: the chronograph's persistence record (S8 data-model §3, FR-008). Pure, host-tested.
// version(1) mode(1) state(1) phase(1) cycles(2) late(1) pad(1) intervalMs(4) endUtcMs(8) remainingMs(4)
// countdownMs(4) workMs(4) breakMs(4) crc16(2), little-endian; CRC-16/CCITT-FALSE as RecBlob. 38 bytes.
#pragma once
#include <cstddef>
#include <cstdint>
#include "core/ChronoModel.h"

namespace core {
namespace ChronoBlob {

constexpr uint8_t kVersion = 1;
constexpr size_t kSize = 38;

size_t encode(const ChronoModel::Raw& r, uint8_t* out);              // writes kSize bytes
bool decode(const uint8_t* buf, size_t len, ChronoModel::Raw& out);  // false (out untouched) on any error

} // namespace ChronoBlob
} // namespace core
