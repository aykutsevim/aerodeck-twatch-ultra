// AeroDeck core: simulated-peripheral-failure mask (data-model §3). Host-tested.
#pragma once
#include <cstdint>
#include "core/Types.h"

namespace core {
namespace SimFail {

constexpr int kBits = 8;

// Bit index for a peripheral, or -1 if it cannot be simulated (core peripherals, etc.).
int bitFor(Periph p);
// Peripheral for a bit index, or Periph::Count if invalid.
Periph periphFor(int bit);
// Lower-case serial name ("lora", "nfc", "gnss", "sd", "mic", "haptics", "imu", "rtc") → bit, or -1.
int bitForName(const char* name);
const char* nameForBit(int bit);
inline bool isSet(uint16_t mask, Periph p)
{
    int b = bitFor(p);
    return b >= 0 && (mask & (1u << b));
}

} // namespace SimFail
} // namespace core
