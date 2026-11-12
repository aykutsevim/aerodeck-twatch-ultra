// AeroDeck core: the flight recorder's days (S6 data-model §2, FR-006..FR-008). Pure, host-tested.
// today + the 7 previous days (newest first); local-midnight rollover; no rollover while the clock is not set or when
// the local date moves backwards; skipped days are inserted as empty records carrying their dates.
#pragma once
#include <cstdint>

namespace core {

constexpr int32_t kUndated = -1;            // today: counted before the clock was ever valid
constexpr int32_t kEmptyDay = INT32_MIN;    // history slot with no data

struct DayRecord {
    int32_t day;       // local day number (days since 1970-01-01 local)
    uint32_t steps;
    uint16_t maxGc;    // Max G in centi-g, 0 = none
    int16_t maxGMin;   // local minute of day of Max G, -1 = none
};

struct RollResult {
    bool rolled;
    uint8_t emptyDays;   // skipped days inserted
    bool dated;          // an undated today got its date
};

class DayLog {
public:
    static constexpr int kHistory = 7;

    DayLog() { clear(); }
    void clear();

    void addSteps(uint32_t n) { today.steps += n; }
    bool notePeak(uint16_t gc, int16_t minuteOfDay);   // true when Max G rose
    RollResult advance(bool clockValid, int32_t localDay);
    // 0 = today, 1..6 = the day `idx` days before today (an empty record with that date when there is no data).
    DayRecord dayAt(int idx) const;

    static int32_t localDayOf(int64_t utcSec, int16_t tzOffsetMin);
    static int minuteOfDay(int64_t utcSec, int16_t tzOffsetMin);
    static uint8_t weekday(int32_t localDay);   // 0 = Monday
    static void civil(int32_t localDay, int& y, int& m, int& d);   // proleptic Gregorian

    DayRecord today;
    DayRecord history[kHistory];

private:
    void push(const DayRecord& r);
};

} // namespace core
