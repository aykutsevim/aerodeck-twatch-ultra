// AeroDeck core: boot-loop protection (data-model §6, research R3). Host-tested.
#pragma once
#include <cstdint>

namespace core {

enum class BootStage : uint8_t { Start, PmuProbe, VendorInit, DisplayInit, UiInit, Stable };
enum class BootMode : uint8_t { Normal, Safe, Headless };

struct BootRecord {   // lives in RTC_NOINIT memory
    uint32_t magic;
    uint8_t fastFailCount;
    BootStage lastStage;
    bool safeMode;
};

namespace BootGuard {
constexpr uint32_t kMagic = 0xAE0D5EC0;
constexpr uint8_t kSafeAfter = 3;       // fast fails before safe mode
constexpr uint8_t kHeadlessAfter = 6;   // fast fails (any stage) before headless
constexpr uint32_t kStableMs = 10000;

// abnormalReset: previous reset was a panic/watchdog/brownout (not power-on or software restart).
// A boot counts as a fast fail if the previous boot reset abnormally before reaching Stable.
BootMode onBoot(BootRecord& r, bool abnormalReset);
void onStable(BootRecord& r);
inline void mark(BootRecord& r, BootStage s) { r.lastStage = s; }
} // namespace BootGuard

} // namespace core
