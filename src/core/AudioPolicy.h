// AeroDeck core: sound gates and the hourly chime rule (S5 data-model §5, FR-009, FR-010, FR-012). Pure, host-tested.
#pragma once
#include <cstdint>
#include "core/SoundQueue.h"

namespace core {

// Quiet hours [start, end) in whole local hours, wrapping past midnight; start == end → none.
bool inQuietHours(int localHour, uint8_t start, uint8_t end);

// nullptr when the sound may play; otherwise "quiet", "volume off" or "quiet hours".
// Quiet mode and volume Off silence everything; quiet hours let only warnings through.
const char* denyReason(SoundPrio p, bool quiet, uint8_t volume, bool inQuietHours);
inline bool allow(SoundPrio p, bool quiet, uint8_t volume, bool inQh) { return denyReason(p, quiet, volume, inQh) == nullptr; }

enum class ChimeReason : uint8_t { Due, Clock, Disabled, Quiet, Volume, NotHh00, Already, QuietHours };
const char* chimeReasonName(ChimeReason r);   // "-", "clock", "disabled", "quiet", "volume", "not-hh00", "already", "quiet-hours"

struct ChimeDecision {
    bool due;
    int64_t hourKey;   // floor(localSec / 3600): unique per local hour
    ChimeReason reason;
};

constexpr int kChimeWindowSec = 5;   // due only in hh:00:00..hh:00:04

int localHourOf(int64_t localSec);   // 0..23
ChimeDecision chimeDue(int64_t localSec, bool clockValid, bool enabled, bool quiet, uint8_t volume, uint8_t qhStart,
                       uint8_t qhEnd, int64_t lastHourKey);

} // namespace core
