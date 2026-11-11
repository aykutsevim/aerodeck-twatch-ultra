// AeroDeck core: POSIX TZ rules (e.g. "GMT0BST,M3.5.0/1,M10.5.0") → UTC offset at an instant (research R3). Pure.
#pragma once
#include <cstdint>

namespace core {

class PosixTz {
public:
    // Supports: std offset [dst [offset] ,Mm.w.d[/time],Mm.w.d[/time]]; names alphabetic (≥3) or <quoted>.
    bool parse(const char* tz);
    int16_t offsetMinutes(int64_t utcSec) const;   // local = UTC + offset
    bool isDst(int64_t utcSec) const;
    bool hasDst() const { return hasDst_; }
    int16_t stdOffsetMin() const { return int16_t(stdOff_ / 60); }

private:
    struct Rule {
        uint8_t month;     // 1..12
        uint8_t week;      // 1..5 (5 = last)
        uint8_t weekday;   // 0 = Sunday
        int32_t timeSec;   // local time of day of the change (may be negative or > 24 h)
    };
    int64_t ruleUtc(const Rule& r, int year, int32_t localOffsetSec) const;

    int32_t stdOff_ = 0;   // seconds east of UTC
    int32_t dstOff_ = 0;
    bool hasDst_ = false;
    Rule start_{}, end_{};
};

} // namespace core
