// AeroDeck core: haptic pattern selection and priority (S4 data-model §4, research R2). Pure, host-tested.
// Effect ids are DRV2605 ROM library 1 (ERM) waveforms; 0x80|n in a sequence = wait n x 10 ms.
#pragma once
#include <cstdint>

namespace core {

enum class Pattern : uint8_t { None, Click, Advisory, Caution, StickShaker };
enum class Intensity : uint8_t { Off, Low, High };

struct Seq {
    uint8_t n;          // entries used in w (<= 8)
    uint8_t w[8];
    uint16_t lengthMs;  // nominal play time (measured on the device, validation.md)
    uint8_t prio;       // Click 1, Advisory 2, Caution 3, StickShaker 4
};

namespace HapticPolicy {
constexpr uint8_t wait10ms(uint8_t n) { return uint8_t(0x80 | n); }
// False when nothing must play: Off → no clicks/advisory/caution; quiet → no advisory/caution. The stick shaker
// (WARNING) always plays at full strength.
bool select(Pattern p, Intensity i, bool quiet, Seq& out);
uint8_t priority(Pattern p);
// A new pattern starts when the current one has ended or the new one is at least as important. Never queued.
bool shouldStart(uint8_t newPrio, uint8_t curPrio, uint32_t curEndMs, uint32_t nowMs);
} // namespace HapticPolicy

} // namespace core
