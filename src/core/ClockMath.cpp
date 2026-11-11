#include "core/ClockMath.h"

namespace core {

// Howard Hinnant's days_from_civil / civil_from_days (public domain algorithms).
static int64_t daysFromCivil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = unsigned(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + int64_t(doe) - 719468;
}

static void civilFromDays(int64_t z, int& y, int& m, int& d)
{
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = unsigned(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t yy = int64_t(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = int(doy - (153 * mp + 2) / 5 + 1);
    m = int(mp < 10 ? mp + 3 : mp - 9);
    y = int(yy + (m <= 2));
}

static int64_t floorDiv(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

int64_t utcFromCivil(int y, int mo, int d, int h, int mi, int s)
{
    return daysFromCivil(y, unsigned(mo), unsigned(d)) * 86400 + h * 3600 + mi * 60 + s;
}

void civilFromUtc(int64_t utc, int& y, int& mo, int& d, int& h, int& mi, int& s)
{
    int64_t days = floorDiv(utc, 86400);
    int64_t rem = utc - days * 86400;
    civilFromDays(days, y, mo, d);
    h = int(rem / 3600);
    mi = int(rem % 3600 / 60);
    s = int(rem % 60);
}

LocalTime splitLocal(int64_t utcSec, uint16_t ms, int16_t tzOffsetMin)
{
    int64_t local = utcSec + int64_t(tzOffsetMin) * 60;
    int64_t sod = local - floorDiv(local, 86400) * 86400;   // seconds of day, 0..86399
    LocalTime t;
    t.h = uint8_t(sod / 3600);
    t.m = uint8_t(sod % 3600 / 60);
    t.s = uint8_t(sod % 60);
    if (ms > 999) {
        ms = 999;
    }
    t.headingDeg = float(t.s) * 6.0f + float(ms) * 0.006f;
    return t;
}

static bool num(const char* p, int n, int& out)
{
    out = 0;
    for (int i = 0; i < n; i++) {
        if (p[i] < '0' || p[i] > '9') {
            return false;
        }
        out = out * 10 + (p[i] - '0');
    }
    return true;
}

static bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

bool parseIso8601Utc(const char* s, int64_t& outUtc)
{
    if (!s) {
        return false;
    }
    // YYYY-MM-DDTHH:MM:SSZ = 20 chars
    for (int i = 0; i < 20; i++) {
        if (s[i] == '\0') {
            return false;
        }
    }
    if (s[20] != '\0' || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' || s[16] != ':' ||
        s[19] != 'Z') {
        return false;
    }
    int y, mo, d, h, mi, se;
    if (!num(s, 4, y) || !num(s + 5, 2, mo) || !num(s + 8, 2, d) || !num(s + 11, 2, h) ||
        !num(s + 14, 2, mi) || !num(s + 17, 2, se)) {
        return false;
    }
    static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (mo < 1 || mo > 12 || h > 23 || mi > 59 || se > 59 || d < 1) {
        return false;
    }
    int dim = mdays[mo - 1] + (mo == 2 && leap(y) ? 1 : 0);
    if (d > dim) {
        return false;
    }
    outUtc = utcFromCivil(y, mo, d, h, mi, se);
    return true;
}

} // namespace core
