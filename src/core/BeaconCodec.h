// AeroDeck core: TCAS beacon v1 (AD-SAS-001 §7.2, S9 data-model §3). Pure, host-tested.
// Layout (little-endian): [0] 0xAD [1] version 0x01 [2] type 0x01 [3..6] node id [7] seq [8..15] callsign (ASCII,
// space-padded) [16] battery % [17] flags (bit0 = position) [18..20] lat int24 [21..23] lon int24 (only with bit0).
#pragma once
#include <cstddef>
#include <cstdint>

namespace core {

struct Beacon {
    uint32_t nodeId;
    uint8_t seq;
    char callsign[9];   // 1..8 chars [A-Z0-9-], NUL-terminated
    uint8_t battPct;
    bool hasPos;
    int32_t lat24, lon24;   // S12; unused in S9
};

namespace BeaconCodec {

constexpr uint8_t kMagic = 0xAD;
constexpr uint8_t kVersion = 0x01;
constexpr uint8_t kTypeBeacon = 0x01;
constexpr uint8_t kFlagPos = 0x01;
constexpr size_t kSize = 18;
constexpr size_t kSizePos = 24;
constexpr size_t kMaxSize = kSizePos;

size_t encode(const Beacon& b, uint8_t* out);                // 18 or 24 bytes; the callsign is uppercased
bool decode(const uint8_t* buf, size_t n, Beacon& out);      // false (out untouched) on any malformed field
bool isMeshtastic(const uint8_t* buf, size_t n);             // FR-TCS-007 hook: always false in S9

} // namespace BeaconCodec
} // namespace core
