#include "core/DayLog.h"

namespace core {

namespace {

int64_t floorDiv(int64_t a, int64_t b)
{
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
    return q;
}

DayRecord blank(int32_t day) { return DayRecord{day, 0, 0, -1}; }

} // namespace

void DayLog::clear()
{
    today = blank(kUndated);
    for (DayRecord& r : history) {
        r = blank(kEmptyDay);
    }
}

bool DayLog::notePeak(uint16_t gc, int16_t minuteOfDay)
{
    if (gc <= today.maxGc) {
        return false;
    }
    today.maxGc = gc;
    today.maxGMin = minuteOfDay;
    return true;
}

void DayLog::push(const DayRecord& r)
{
    for (int i = kHistory - 1; i > 0; i--) {
        history[i] = history[i - 1];
    }
    history[0] = r;
}

RollResult DayLog::advance(bool clockValid, int32_t localDay)
{
    RollResult res{false, 0, false};
    if (!clockValid) {
        return res;
    }
    if (today.day == kUndated) {
        today.day = localDay;
        res.dated = true;
        return res;
    }
    if (localDay <= today.day) {
        return res;   // same day, or the clock / time zone moved the date backwards
    }
    push(today);
    int32_t first = today.day + 1;
    if (localDay - first > kHistory) {
        first = localDay - kHistory;   // only the newest 7 skipped days can still be shown
    }
    for (int32_t d = first; d < localDay; d++) {
        push(blank(d));
        res.emptyDays++;
    }
    today = blank(localDay);
    res.rolled = true;
    return res;
}

DayRecord DayLog::dayAt(int idx) const
{
    if (idx <= 0) {
        return today;
    }
    if (today.day == kUndated) {
        return idx - 1 < kHistory ? history[idx - 1] : blank(kEmptyDay);
    }
    const int32_t want = today.day - idx;
    for (const DayRecord& r : history) {
        if (r.day == want) {
            return r;
        }
    }
    return blank(want);
}

int32_t DayLog::localDayOf(int64_t utcSec, int16_t tzOffsetMin)
{
    return int32_t(floorDiv(utcSec + int64_t(tzOffsetMin) * 60, 86400));
}

int DayLog::minuteOfDay(int64_t utcSec, int16_t tzOffsetMin)
{
    const int64_t local = utcSec + int64_t(tzOffsetMin) * 60;
    return int((local - floorDiv(local, 86400) * 86400) / 60);
}

uint8_t DayLog::weekday(int32_t localDay)
{
    return uint8_t(floorDiv(int64_t(localDay) + 3, 7) * -7 + int64_t(localDay) + 3);   // 1970-01-01 = Thursday (3)
}

void DayLog::civil(int32_t localDay, int& y, int& m, int& d)
{
    // H. Hinnant's civil_from_days
    int64_t z = int64_t(localDay) + 719468;
    const int64_t era = floorDiv(z, 146097);
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    d = int(doy - (153 * mp + 2) / 5 + 1);
    m = int(mp < 10 ? mp + 3 : mp - 9);
    y = int(yoe + era * 400 + (m <= 2 ? 1 : 0));
}

} // namespace core
