#include "core/PosixTz.h"
#include "core/ClockMath.h"

namespace core {

namespace {

bool isAlpha(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
bool isDigit(char c) { return c >= '0' && c <= '9'; }

// Zone abbreviation: <...> or ≥3 letters.
bool parseName(const char*& p)
{
    if (*p == '<') {
        const char* q = p + 1;
        while (*q && *q != '>') q++;
        if (*q != '>' || q == p + 1) return false;
        p = q + 1;
        return true;
    }
    const char* q = p;
    while (isAlpha(*q)) q++;
    if (q - p < 3) return false;
    p = q;
    return true;
}

bool parseNum(const char*& p, int maxDigits, int& out)
{
    if (!isDigit(*p)) return false;
    out = 0;
    int n = 0;
    while (isDigit(*p) && n < maxDigits) {
        out = out * 10 + (*p++ - '0');
        n++;
    }
    return !isDigit(*p);
}

// [+-]hh[:mm[:ss]] → seconds (sign as written).
bool parseHms(const char*& p, int32_t& outSec, int maxHours)
{
    int sign = 1;
    if (*p == '+' || *p == '-') sign = (*p++ == '-') ? -1 : 1;
    int h = 0, m = 0, s = 0;
    if (!parseNum(p, 3, h) || h > maxHours) return false;
    if (*p == ':') {
        p++;
        if (!parseNum(p, 2, m) || m > 59) return false;
        if (*p == ':') {
            p++;
            if (!parseNum(p, 2, s) || s > 59) return false;
        }
    }
    outSec = sign * (h * 3600 + m * 60 + s);
    return true;
}

int daysInMonth(int y, int m)
{
    static const int k[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return k[m - 1] + (m == 2 && leap ? 1 : 0);
}

} // namespace

bool PosixTz::parse(const char* tz)
{
    *this = PosixTz();
    if (!tz || !*tz) return false;
    const char* p = tz;
    if (!parseName(p)) return false;
    int32_t off;
    if (!parseHms(p, off, 24)) return false;
    stdOff_ = -off;   // POSIX offsets are positive west of Greenwich
    if (!*p) return true;

    if (!parseName(p)) return false;
    hasDst_ = true;
    dstOff_ = stdOff_ + 3600;
    if (*p && *p != ',') {
        if (!parseHms(p, off, 24)) return false;
        dstOff_ = -off;
    }
    Rule* rules[2] = {&start_, &end_};
    for (Rule* r : rules) {
        if (*p != ',') return false;   // DST requires explicit start and end rules
        p++;
        if (*p != 'M') return false;   // only the Mm.w.d form is supported
        p++;
        int mo, w, d;
        if (!parseNum(p, 2, mo) || *p != '.' || mo < 1 || mo > 12) return false;
        p++;
        if (!parseNum(p, 1, w) || *p != '.' || w < 1 || w > 5) return false;
        p++;
        if (!parseNum(p, 1, d) || d > 6) return false;
        r->month = uint8_t(mo);
        r->week = uint8_t(w);
        r->weekday = uint8_t(d);
        r->timeSec = 2 * 3600;   // default 02:00
        if (*p == '/') {
            p++;
            if (!parseHms(p, r->timeSec, 167)) return false;
        }
    }
    return *p == '\0';
}

int64_t PosixTz::ruleUtc(const Rule& r, int year, int32_t localOffsetSec) const
{
    const int64_t first = utcFromCivil(year, r.month, 1, 0, 0, 0);
    const int64_t days = first / 86400;   // epoch is midnight, so this is exact for y ≥ 1970
    const int wdFirst = int(((days % 7) + 7 + 4) % 7);   // 1970-01-01 was a Thursday (4)
    int day = 1 + (r.weekday - wdFirst + 7) % 7 + (r.week - 1) * 7;
    while (day > daysInMonth(year, r.month)) day -= 7;   // week 5 = last
    return first + int64_t(day - 1) * 86400 + r.timeSec - localOffsetSec;
}

bool PosixTz::isDst(int64_t utc) const
{
    if (!hasDst_) return false;
    int y, mo, d, h, mi, s;
    civilFromUtc(utc + stdOff_, y, mo, d, h, mi, s);
    const int64_t start = ruleUtc(start_, y, stdOff_);   // start time is in local standard time
    const int64_t end = ruleUtc(end_, y, dstOff_);       // end time is in local daylight time
    if (start < end) {
        return utc >= start && utc < end;                // northern hemisphere
    }
    return !(utc >= end && utc < start);                 // southern hemisphere
}

int16_t PosixTz::offsetMinutes(int64_t utc) const
{
    return int16_t((isDst(utc) ? dstOff_ : stdOff_) / 60);
}

} // namespace core
