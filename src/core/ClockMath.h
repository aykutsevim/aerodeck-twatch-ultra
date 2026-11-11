// AeroDeck core: UTC/local time math for the PFD (data-model §3). Pure, host-tested.
#pragma once
#include <cstdint>

namespace core {

struct LocalTime {
    uint8_t h, m, s;
    float headingDeg;   // seconds as heading: s*6 + ms*0.006, in [0, 360)
};

// Local wall time from UTC seconds + milliseconds and an offset in minutes.
LocalTime splitLocal(int64_t utcSec, uint16_t ms, int16_t tzOffsetMin);

// Strict "YYYY-MM-DDTHH:MM:SSZ". Returns false on any malformed or impossible date/time.
bool parseIso8601Utc(const char* s, int64_t& outUtc);

void civilFromUtc(int64_t utc, int& y, int& mo, int& d, int& h, int& mi, int& s);
int64_t utcFromCivil(int y, int mo, int d, int h, int mi, int s);

// A clock year we trust (rejects RTC defaults such as 2000 and garbage).
inline bool clockPlausible(int year) { return year >= 2025 && year <= 2099; }

} // namespace core
