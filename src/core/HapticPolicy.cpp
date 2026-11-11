#include "core/HapticPolicy.h"

namespace core {
namespace HapticPolicy {

namespace {
// Effect ids (DRV2605 library 1). Tuned on the device: see specs/005-alert-haptics/validation.md.
constexpr uint8_t kClickHigh = 4;      // Sharp Click 100 %
constexpr uint8_t kClickLow = 5;       // Sharp Click 60 %
constexpr uint8_t kAdvisoryHigh = 7;   // Soft Bump 100 %
constexpr uint8_t kAdvisoryLow = 8;    // Soft Bump 60 %
constexpr uint8_t kCautionHigh = 12;   // Triple Click 100 %
constexpr uint8_t kCautionLow = 11;    // Double Click 60 %
constexpr uint8_t kShaker = 52;        // Pulsing Strong 1 100 %

constexpr uint16_t kClickMs = 30;
constexpr uint16_t kAdvisoryMs = 100;
constexpr uint16_t kCautionMs = 400;
constexpr uint16_t kShakerMs = 1500;

void one(Seq& s, uint8_t w, uint16_t ms, uint8_t prio)
{
    s = Seq{};
    s.n = 1;
    s.w[0] = w;
    s.lengthMs = ms;
    s.prio = prio;
}
} // namespace

uint8_t priority(Pattern p)
{
    switch (p) {
    case Pattern::Click: return 1;
    case Pattern::Advisory: return 2;
    case Pattern::Caution: return 3;
    case Pattern::StickShaker: return 4;
    default: return 0;
    }
}

bool select(Pattern p, Intensity i, bool quiet, Seq& out)
{
    const bool high = i == Intensity::High;
    switch (p) {
    case Pattern::Click:
        if (i == Intensity::Off) return false;
        one(out, high ? kClickHigh : kClickLow, kClickMs, priority(p));
        return true;
    case Pattern::Advisory:
        if (i == Intensity::Off || quiet) return false;
        one(out, high ? kAdvisoryHigh : kAdvisoryLow, kAdvisoryMs, priority(p));
        return true;
    case Pattern::Caution:
        if (i == Intensity::Off || quiet) return false;
        one(out, high ? kCautionHigh : kCautionLow, kCautionMs, priority(p));
        return true;
    case Pattern::StickShaker:
        out = Seq{};
        out.n = 5;
        out.w[0] = kShaker;
        out.w[1] = wait10ms(10);
        out.w[2] = kShaker;
        out.w[3] = wait10ms(10);
        out.w[4] = kShaker;
        out.lengthMs = kShakerMs;
        out.prio = priority(p);
        return true;
    default:
        return false;
    }
}

bool shouldStart(uint8_t newPrio, uint8_t curPrio, uint32_t curEndMs, uint32_t nowMs)
{
    return int32_t(nowMs - curEndMs) >= 0 || newPrio >= curPrio;
}

} // namespace HapticPolicy
} // namespace core
