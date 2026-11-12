#include "core/AudioPolicy.h"

namespace core {

namespace {

int64_t floorDiv(int64_t a, int64_t b)
{
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
    return q;
}

} // namespace

bool inQuietHours(int h, uint8_t start, uint8_t end)
{
    if (start == end) {
        return false;
    }
    if (start < end) {
        return h >= start && h < end;
    }
    return h >= start || h < end;
}

const char* denyReason(SoundPrio p, bool quiet, uint8_t volume, bool inQh)
{
    if (quiet) return "quiet";
    if (volume == 0) return "volume off";
    if (inQh && p != SoundPrio::Warning) return "quiet hours";
    return nullptr;
}

const char* chimeReasonName(ChimeReason r)
{
    switch (r) {
    case ChimeReason::Due: return "-";
    case ChimeReason::Clock: return "clock";
    case ChimeReason::Disabled: return "disabled";
    case ChimeReason::Quiet: return "quiet";
    case ChimeReason::Volume: return "volume";
    case ChimeReason::NotHh00: return "not-hh00";
    case ChimeReason::Already: return "already";
    case ChimeReason::QuietHours: return "quiet-hours";
    }
    return "?";
}

int localHourOf(int64_t localSec) { return int(floorDiv(localSec, 3600) - floorDiv(localSec, 86400) * 24); }

ChimeDecision chimeDue(int64_t localSec, bool clockValid, bool enabled, bool quiet, uint8_t volume, uint8_t qhStart,
                       uint8_t qhEnd, int64_t lastHourKey)
{
    const int64_t key = floorDiv(localSec, 3600);
    ChimeDecision d{false, key, ChimeReason::Due};
    if (!clockValid) d.reason = ChimeReason::Clock;
    else if (!enabled) d.reason = ChimeReason::Disabled;
    else if (quiet) d.reason = ChimeReason::Quiet;
    else if (volume == 0) d.reason = ChimeReason::Volume;
    else if (localSec - key * 3600 >= kChimeWindowSec) d.reason = ChimeReason::NotHh00;
    else if (key == lastHourKey) d.reason = ChimeReason::Already;
    else if (inQuietHours(localHourOf(localSec), qhStart, qhEnd)) d.reason = ChimeReason::QuietHours;
    else d.due = true;
    return d;
}

} // namespace core
