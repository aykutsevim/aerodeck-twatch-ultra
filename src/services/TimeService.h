// AeroDeck clock: RTC (UTC) → system time, validity, CLOCK NOT SET, `ad time` (research R4).
#pragma once
#include <cstdint>
#include "services/Snapshot.h"

struct ClockSnapshot {
    bool valid;            // RTC integrity guaranteed and plausible year
    int64_t utc;           // seconds since epoch (1 Hz)
    int16_t tzOffsetMin;   // setting `tzoff`
};

namespace TimeService {
void begin();                    // after board().begin(); registers `ad time`
const Snapshot<ClockSnapshot>& clock();
bool set(int64_t utc);           // writes RTC + system clock
void tick(uint32_t nowMs);       // svc task, 1 Hz
} // namespace TimeService
