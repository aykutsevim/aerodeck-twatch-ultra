#include "core/Metar.h"
#include <cstdio>
#include <cstring>
#include "core/ClockMath.h"

namespace core {
namespace Metar {

namespace {

constexpr int kMaxTokens = 64;

bool allDigits(const char* s, size_t n)
{
    if (n == 0) {
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
    }
    return true;
}

bool allSlash(const char* s, size_t n)
{
    if (n == 0) {
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        if (s[i] != '/') {
            return false;
        }
    }
    return true;
}

int num(const char* s, size_t n)
{
    int v = 0;
    for (size_t i = 0; i < n; i++) {
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

bool endsWith(const char* s, size_t n, const char* suf)
{
    size_t m = strlen(suf);
    return n >= m && memcmp(s + n - m, suf, m) == 0;
}

void resetReport(Report& r)
{
    memset(&r, 0, sizeof r);
    r.day = r.hour = r.minute = -1;
    r.windDir = r.windSpdKt = r.windGustKt = kMissing;
    r.windVarFrom = r.windVarTo = kMissing;
    r.visM = -1;
    r.tempC = r.dewC = kNoTemp;
    r.qnhHpa = kMissing;
    r.noCloud = NoCloud::Unset;
}

// dddff(f)(Gff(f))KT|MPS, VRBff, /////KT. Returns true when the token is a wind group (valid or not).
bool parseWind(const char* t, size_t n, Report& r)
{
    size_t u;
    bool mps;
    if (endsWith(t, n, "KT")) {
        u = 2;
        mps = false;
    } else if (endsWith(t, n, "MPS")) {
        u = 3;
        mps = true;
    } else {
        return false;
    }
    const size_t b = n - u;
    if (b < 5) {
        return false;
    }
    if (allSlash(t, b)) {
        return true;   // wind not reported: stays missing
    }
    int dir;
    if (memcmp(t, "VRB", 3) == 0) {
        dir = kWindVrb;
    } else if (allDigits(t, 3)) {
        dir = num(t, 3);
    } else {
        return false;
    }
    // speed: 2–3 digits, optional G + 2–3 digits
    size_t i = 3;
    size_t s0 = i;
    while (i < b && t[i] >= '0' && t[i] <= '9' && i - s0 < 3) {
        i++;
    }
    const size_t sl = i - s0;
    if (sl < 2) {
        return false;
    }
    int spd = num(t + s0, sl);
    int gust = -1;
    if (i < b) {
        if (t[i] != 'G') {
            return false;
        }
        i++;
        size_t g0 = i;
        while (i < b && t[i] >= '0' && t[i] <= '9' && i - g0 < 3) {
            i++;
        }
        if (i - g0 < 2 || i != b) {
            return false;
        }
        gust = num(t + g0, i - g0);
    }
    if (mps) {
        spd = (spd * 1944 + 500) / 1000;
        if (gust >= 0) {
            gust = (gust * 1944 + 500) / 1000;
        }
    }
    if ((dir != kWindVrb && (dir < 0 || dir > 360)) || spd > 250 || gust > 250) {
        return true;   // a wind group, but out of range: missing
    }
    r.windDir = int16_t(dir);
    r.windSpdKt = int16_t(spd);
    r.windGustKt = int16_t(gust);
    return true;
}

bool parseSector(const char* t, size_t n, Report& r)
{
    if (n != 7 || t[3] != 'V' || !allDigits(t, 3) || !allDigits(t + 4, 3)) {
        return false;
    }
    int a = num(t, 3), b = num(t + 4, 3);
    if (a > 360 || b > 360) {
        return true;
    }
    r.windVarFrom = int16_t(a);
    r.windVarTo = int16_t(b);
    return true;
}

// "n", "n/d" (no unit) → sixteenths; false when not a number of this form.
bool parseSmNumber(const char* s, size_t n, int& v16)
{
    const char* slash = static_cast<const char*>(memchr(s, '/', n));
    if (!slash) {
        if (n > 2 || !allDigits(s, n)) {
            return false;
        }
        v16 = num(s, n) * 16;
        return true;
    }
    size_t a = size_t(slash - s), b = n - a - 1;
    if (a == 0 || a > 2 || b == 0 || b > 2 || !allDigits(s, a) || !allDigits(slash + 1, b)) {
        return false;
    }
    int nu = num(s, a), de = num(slash + 1, b);
    if (de != 2 && de != 4 && de != 8 && de != 16) {
        return false;
    }
    v16 = nu * 16 / de;
    return true;
}

void smText(Report& r)
{
    const int w = r.visSm16 / 16, f = r.visSm16 % 16;
    char frac[8] = "";
    if (f) {
        int nu = f, de = 16;
        while (nu % 2 == 0 && de > 1) {
            nu /= 2;
            de /= 2;
        }
        snprintf(frac, sizeof frac, "%d/%d", nu, de);
    }
    const char* pre = r.visQual < 0 ? "<" : "";
    const char* post = r.visQual > 0 ? "+" : "";
    if (w && f) {
        snprintf(r.visText, sizeof r.visText, "%s%d %s%s SM", pre, w, frac, post);
    } else if (f) {
        snprintf(r.visText, sizeof r.visText, "%s%s%s SM", pre, frac, post);
    } else {
        snprintf(r.visText, sizeof r.visText, "%s%d%s SM", pre, w, post);
    }
}

void setSm(Report& r, int v16, int8_t qual)
{
    if (v16 < 0 || v16 > 99 * 16) {
        return;
    }
    r.visIsSm = true;
    r.visSm16 = uint16_t(v16);
    r.visQual = qual;
    r.visM = int32_t((int64_t(v16) * 1609344 + 8000) / 16000);
    smText(r);
}

void setMetres(Report& r, int m)
{
    if (m == 9999) {
        r.visM = 10000;
        snprintf(r.visText, sizeof r.visText, "10 km+");
    } else {
        r.visM = m;
        if (m < 5000) {
            snprintf(r.visText, sizeof r.visText, "%d m", m);
        } else if (m % 1000 == 0) {
            snprintf(r.visText, sizeof r.visText, "%d km", m / 1000);
        } else {
            snprintf(r.visText, sizeof r.visText, "%d.%d km", m / 1000, (m % 1000) / 100);
        }
    }
}

bool parseCloud(const char* t, size_t n, Report& r)
{
    Cover c;
    size_t p;
    if (n >= 5 && memcmp(t, "VV", 2) == 0) {
        c = Cover::Vv;
        p = 2;
    } else if (n >= 6) {
        if (memcmp(t, "FEW", 3) == 0) {
            c = Cover::Few;
        } else if (memcmp(t, "SCT", 3) == 0) {
            c = Cover::Sct;
        } else if (memcmp(t, "BKN", 3) == 0) {
            c = Cover::Bkn;
        } else if (memcmp(t, "OVC", 3) == 0) {
            c = Cover::Ovc;
        } else if (memcmp(t, "///", 3) == 0) {
            c = Cover::Unknown;
        } else {
            return false;
        }
        p = 3;
    } else {
        return false;
    }
    int base;
    if (allDigits(t + p, 3)) {
        base = num(t + p, 3) * 100;
    } else if (allSlash(t + p, 3)) {
        base = -1;
    } else {
        return false;
    }
    const char* suf = t + p + 3;
    const size_t sl = n - p - 3;
    bool cb = false, tcu = false;
    if (sl == 2 && memcmp(suf, "CB", 2) == 0) {
        cb = true;
    } else if (sl == 3 && memcmp(suf, "TCU", 3) == 0) {
        tcu = true;
    } else if (sl == 3 && allSlash(suf, 3)) {
        // cloud type not observed (AUTO)
    } else if (sl != 0) {
        return false;
    }
    if (c == Cover::Vv && sl != 0) {
        return false;
    }
    if (c == Cover::Unknown && base < 0 && !cb && !tcu) {
        return true;   // "//////": nothing usable
    }
    if (r.nClouds < kMaxClouds) {
        Cloud& k = r.clouds[r.nClouds++];
        k.cover = c;
        k.baseFt = int16_t(base);
        k.cb = cb || tcu;
        k.tcu = tcu;
    }
    return true;
}

bool parseTemp(const char* t, size_t n, Report& r)
{
    if (n == 5 && allSlash(t, 5)) {
        return true;   // both missing
    }
    const char* slash = static_cast<const char*>(memchr(t, '/', n));
    if (!slash) {
        return false;
    }
    auto part = [](const char* s, size_t m, int8_t& out) -> bool {
        if (m == 2 && allSlash(s, 2)) {
            out = kNoTemp;
            return true;
        }
        bool neg = false;
        if (m == 3 && s[0] == 'M') {
            neg = true;
            s++;
            m--;
        }
        if (m != 2 || !allDigits(s, 2)) {
            return false;
        }
        int v = num(s, 2);
        out = int8_t(neg ? -v : v);
        return true;
    };
    int8_t a, b = kNoTemp;
    const size_t la = size_t(slash - t), lb = n - la - 1;
    if (!part(t, la, a)) {
        return false;
    }
    if (lb != 0 && !part(slash + 1, lb, b)) {
        return false;
    }
    r.tempC = (a != kNoTemp && a >= -80 && a <= 60) ? a : kNoTemp;
    r.dewC = (b != kNoTemp && b >= -80 && b <= 60) ? b : kNoTemp;
    return true;
}

bool parsePressure(const char* t, size_t n, Report& r)
{
    if (n != 5 || (t[0] != 'Q' && t[0] != 'A')) {
        return false;
    }
    if (allSlash(t + 1, 4)) {
        return true;
    }
    if (!allDigits(t + 1, 4)) {
        return false;
    }
    const int v = num(t + 1, 4);
    int hpa = v;
    bool inHg = false;
    if (t[0] == 'A') {
        hpa = int((int64_t(v) * 338639 + 500000) / 1000000);
        inHg = true;
    }
    if (hpa >= 850 && hpa <= 1090) {
        r.qnhHpa = int16_t(hpa);
        r.qnhWasInHg = inHg;
    }
    return true;
}

bool isStop(const char* t, size_t n)
{
    return (n == 3 && memcmp(t, "RMK", 3) == 0) || (n == 5 && memcmp(t, "NOSIG", 5) == 0) ||
           (n == 5 && memcmp(t, "BECMG", 5) == 0) || (n == 5 && memcmp(t, "TEMPO", 5) == 0);
}

bool isStationToken(const char* t, size_t n)
{
    if (n != 4 || t[0] < 'A' || t[0] > 'Z') {
        return false;
    }
    for (size_t i = 1; i < 4; i++) {
        if (!((t[i] >= 'A' && t[i] <= 'Z') || (t[i] >= '0' && t[i] <= '9'))) {
            return false;
        }
    }
    return true;
}

} // namespace

bool decode(const char* raw, Report& r)
{
    resetReport(r);
    if (!raw) {
        return false;
    }
    char buf[kMaxRaw + 1];
    size_t len = strnlen(raw, kMaxRaw);
    memcpy(buf, raw, len);
    buf[len] = 0;

    const char* tok[kMaxTokens];
    size_t tl[kMaxTokens];
    int nt = 0;
    for (size_t i = 0; i < len && nt < kMaxTokens;) {
        while (i < len && (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\r' || buf[i] == '\n')) {
            i++;
        }
        if (i >= len) {
            break;
        }
        size_t s = i;
        while (i < len && buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\r' && buf[i] != '\n') {
            i++;
        }
        size_t n = i - s;
        while (n > 0 && (buf[s + n - 1] == '=')) {
            n--;   // end-of-report marker
        }
        if (n > 0) {
            tok[nt] = buf + s;
            tl[nt] = n;
            nt++;
        }
    }

    int k = 0;
    if (k < nt && ((tl[k] == 5 && memcmp(tok[k], "METAR", 5) == 0) || (tl[k] == 5 && memcmp(tok[k], "SPECI", 5) == 0))) {
        k++;
    }
    if (k >= nt || !isStationToken(tok[k], tl[k])) {
        return false;
    }
    memcpy(r.station, tok[k], 4);
    r.station[4] = 0;
    k++;
    if (k >= nt || tl[k] != 7 || tok[k][6] != 'Z' || !allDigits(tok[k], 6)) {
        return false;
    }
    const int d = num(tok[k], 2), h = num(tok[k] + 2, 2), m = num(tok[k] + 4, 2);
    if (d < 1 || d > 31 || h > 23 || m > 59) {
        return false;
    }
    r.day = int8_t(d);
    r.hour = int8_t(h);
    r.minute = int8_t(m);
    k++;

    bool windSeen = false, visSeen = false;
    for (; k < nt; k++) {
        const char* t = tok[k];
        const size_t n = tl[k];
        if (isStop(t, n)) {
            break;
        }
        if (n == 3 && memcmp(t, "NIL", 3) == 0) {
            r.nil = true;
            break;
        }
        if ((n == 4 && memcmp(t, "AUTO", 4) == 0) || (n == 3 && memcmp(t, "COR", 3) == 0)) {
            continue;
        }
        if (!windSeen && parseWind(t, n, r)) {
            windSeen = true;
            continue;
        }
        if (windSeen && r.windVarFrom < 0 && parseSector(t, n, r)) {
            continue;
        }
        if (!visSeen) {
            if (n == 5 && memcmp(t, "CAVOK", 5) == 0) {
                r.cavok = true;
                r.visM = 10000;
                snprintf(r.visText, sizeof r.visText, "CAVOK");
                visSeen = true;
                continue;
            }
            if (n == 4 && allDigits(t, 4)) {
                setMetres(r, num(t, 4));
                visSeen = true;
                continue;
            }
            if (n == 4 && allSlash(t, 4)) {
                visSeen = true;   // visibility not reported
                continue;
            }
            // whole statute miles followed by a fraction: "1 1/2SM"
            if (n <= 2 && allDigits(t, n) && k + 1 < nt && endsWith(tok[k + 1], tl[k + 1], "SM")) {
                int f16;
                if (memchr(tok[k + 1], '/', tl[k + 1]) && parseSmNumber(tok[k + 1], tl[k + 1] - 2, f16) && f16 < 16) {
                    setSm(r, num(t, n) * 16 + f16, 0);
                    visSeen = true;
                    k++;
                    continue;
                }
            }
            if (n >= 3 && endsWith(t, n, "SM")) {
                const char* s = t;
                size_t sn = n - 2;
                int8_t q = 0;
                if (s[0] == 'M' || s[0] == 'P') {
                    q = s[0] == 'M' ? -1 : 1;
                    s++;
                    sn--;
                }
                int v16;
                if (parseSmNumber(s, sn, v16)) {
                    setSm(r, v16, q);
                    visSeen = true;
                    continue;
                }
            }
        }
        if (n == 5 && memcmp(t, "CAVOK", 5) == 0) {
            continue;
        }
        if (n == 3 && memcmp(t, "NSC", 3) == 0) {
            r.noCloud = NoCloud::Nsc;
            continue;
        }
        if (n == 3 && memcmp(t, "NCD", 3) == 0) {
            r.noCloud = NoCloud::Ncd;
            continue;
        }
        if (n == 3 && memcmp(t, "SKC", 3) == 0) {
            r.noCloud = NoCloud::Skc;
            continue;
        }
        if (n == 3 && memcmp(t, "CLR", 3) == 0) {
            r.noCloud = NoCloud::Clr;
            continue;
        }
        if (parseCloud(t, n, r)) {
            continue;
        }
        if (parsePressure(t, n, r)) {
            continue;
        }
        if (t[0] != 'R' && parseTemp(t, n, r)) {
            continue;
        }
        // anything else (weather, RVR, runway state, NSW, ...) is skipped
    }
    return true;
}

int32_t ceilingFt(const Report& r)
{
    bool info = r.cavok || r.noCloud != NoCloud::Unset;
    bool unknownCeil = false;
    int32_t best = kNoCeiling;
    for (int i = 0; i < r.nClouds && i < kMaxClouds; i++) {
        const Cloud& c = r.clouds[i];
        if (c.cover == Cover::Unknown) {
            continue;
        }
        info = true;
        if (c.cover == Cover::Bkn || c.cover == Cover::Ovc || c.cover == Cover::Vv) {
            if (c.baseFt < 0) {
                unknownCeil = true;
            } else if (c.baseFt < best) {
                best = c.baseFt;
            }
        }
    }
    if (unknownCeil || !info) {
        return kCeilingUnknown;
    }
    return best;
}

Category category(const Report& r)
{
    Category vis;
    if (r.cavok) {
        vis = Category::Vfr;
    } else if (r.visIsSm) {
        int v = int(r.visSm16) - (r.visQual < 0 ? 1 : 0);
        vis = v < 16 ? Category::Lifr : v < 48 ? Category::Ifr : v <= 80 ? Category::Mvfr : Category::Vfr;
    } else if (r.visM >= 0) {
        const int32_t m = r.visM;
        vis = m < 1600 ? Category::Lifr : m < 4800 ? Category::Ifr : m <= 8000 ? Category::Mvfr : Category::Vfr;
    } else {
        return Category::Unknown;
    }
    const int32_t c = ceilingFt(r);
    if (c == kCeilingUnknown) {
        return vis == Category::Lifr ? Category::Lifr : Category::Unknown;
    }
    const Category ceil = c == kNoCeiling ? Category::Vfr
                          : c < 500       ? Category::Lifr
                          : c < 1000      ? Category::Ifr
                          : c <= 3000     ? Category::Mvfr
                                          : Category::Vfr;
    return uint8_t(ceil) > uint8_t(vis) ? ceil : vis;
}

const char* categoryName(Category c)
{
    switch (c) {
    case Category::Vfr: return "VFR";
    case Category::Mvfr: return "MVFR";
    case Category::Ifr: return "IFR";
    case Category::Lifr: return "LIFR";
    default: return "--";
    }
}

static int daysInMonth(int y, int m)
{
    static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) {
        return 29;
    }
    return kDays[m - 1];
}

int64_t resolveObsTime(int day, int hour, int minute, int64_t nowUtc)
{
    if (nowUtc <= 0 || day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return 0;
    }
    int y, mo, d, h, mi, s;
    civilFromUtc(nowUtc, y, mo, d, h, mi, s);
    for (int back = 0; back < 3; back++) {
        int yy = y, mm = mo - back;
        while (mm < 1) {
            mm += 12;
            yy--;
        }
        if (day > daysInMonth(yy, mm)) {
            continue;
        }
        const int64_t t = utcFromCivil(yy, mm, day, hour, minute, 0);
        if (t <= nowUtc + 600) {
            return t;
        }
    }
    return 0;
}

bool isStale(int64_t obsUtc, int64_t nowUtc)
{
    return obsUtc <= 0 || nowUtc <= 0 || nowUtc - obsUtc > kStaleAfterS;
}

void formatWind(const Report& r, char* out, size_t n)
{
    if (!out || n == 0) {
        return;
    }
    if (r.windSpdKt < 0) {
        snprintf(out, n, "--");
        return;
    }
    if (r.windSpdKt == 0 && (r.windDir == 0 || r.windDir == kWindVrb)) {
        snprintf(out, n, "CALM");
        return;
    }
    char g[8] = "";
    if (r.windGustKt >= 0) {
        snprintf(g, sizeof g, "G%02d", r.windGustKt);
    }
    if (r.windVarFrom >= 0) {
        snprintf(out, n, "VRB %03d-%03d\xC2\xB0 %02d%s kt", r.windVarFrom, r.windVarTo, r.windSpdKt, g);
    } else if (r.windDir == kWindVrb) {
        snprintf(out, n, "VRB %02d%s kt", r.windSpdKt, g);
    } else if (r.windDir < 0) {
        snprintf(out, n, "%02d%s kt", r.windSpdKt, g);
    } else {
        snprintf(out, n, "%03d\xC2\xB0 %02d%s kt", r.windDir, r.windSpdKt, g);
    }
}

void formatClouds(const Report& r, char* out, size_t n)
{
    if (!out || n == 0) {
        return;
    }
    out[0] = 0;
    if (r.cavok) {
        snprintf(out, n, "CAVOK");
        return;
    }
    static const char* const kCover[] = {"FEW", "SCT", "BKN", "OVC", "VV"};
    char tmp[48] = "";
    size_t used = 0;
    int shown = 0;
    for (int i = 0; i < r.nClouds && i < kMaxClouds && shown < 3; i++) {
        const Cloud& c = r.clouds[i];
        if (c.cover == Cover::Unknown) {
            continue;
        }
        char base[4];
        if (c.baseFt < 0) {
            snprintf(base, sizeof base, "///");
        } else {
            snprintf(base, sizeof base, "%03d", c.baseFt / 100);
        }
        const char* suf = c.tcu ? "TCU" : c.cb ? "CB" : "";
        int w = snprintf(tmp + used, sizeof tmp - used, "%s%s%s%s", shown ? " " : "", kCover[int(c.cover)], base, suf);
        if (w < 0 || size_t(w) >= sizeof tmp - used) {
            break;
        }
        used += size_t(w);
        shown++;
    }
    if (shown) {
        snprintf(out, n, "%s", tmp);
        return;
    }
    switch (r.noCloud) {
    case NoCloud::Nsc: snprintf(out, n, "NSC"); break;
    case NoCloud::Ncd: snprintf(out, n, "NCD"); break;
    case NoCloud::Skc: snprintf(out, n, "SKC"); break;
    case NoCloud::Clr: snprintf(out, n, "CLR"); break;
    default: snprintf(out, n, "--"); break;
    }
}

static void feet(int32_t ft, char* out, size_t n)
{
    if (ft >= 1000) {
        snprintf(out, n, "%ld,%03ld ft", long(ft / 1000), long(ft % 1000));
    } else {
        snprintf(out, n, "%ld ft", long(ft));
    }
}

void formatCloudsText(const Report& r, char* out, size_t n)
{
    if (!out || n == 0) {
        return;
    }
    out[0] = 0;
    if (r.cavok) {
        snprintf(out, n, "CAVOK: no cloud below 5,000 ft");
        return;
    }
    static const char* const kWord[] = {"few", "scattered", "broken", "overcast"};
    char tmp[96] = "";
    size_t used = 0;
    int shown = 0;
    for (int i = 0; i < r.nClouds && i < kMaxClouds && shown < 3; i++) {
        const Cloud& c = r.clouds[i];
        if (c.cover == Cover::Unknown) {
            continue;
        }
        char h[16];
        if (c.baseFt < 0) {
            snprintf(h, sizeof h, "height ?");
        } else {
            feet(c.baseFt, h, sizeof h);
        }
        char part[48];
        if (c.cover == Cover::Vv) {
            snprintf(part, sizeof part, "sky obscured, vertical visibility %s", h);
        } else {
            snprintf(part, sizeof part, "%s %s%s", kWord[int(c.cover)], h, c.tcu ? " (TCU)" : c.cb ? " (CB)" : "");
        }
        int w = snprintf(tmp + used, sizeof tmp - used, "%s%s", shown ? ", " : "", part);
        if (w < 0 || size_t(w) >= sizeof tmp - used) {
            break;
        }
        used += size_t(w);
        shown++;
    }
    if (shown) {
        if (tmp[0] >= 'a' && tmp[0] <= 'z') {
            tmp[0] = char(tmp[0] - 32);   // sentence case
        }
        snprintf(out, n, "%s", tmp);
        return;
    }
    switch (r.noCloud) {
    case NoCloud::Nsc: snprintf(out, n, "No significant cloud"); break;
    case NoCloud::Ncd: snprintf(out, n, "No cloud detected"); break;
    case NoCloud::Skc:
    case NoCloud::Clr: snprintf(out, n, "Sky clear"); break;
    default: snprintf(out, n, "--"); break;
    }
}

void formatTemp(const Report& r, char* out, size_t n)
{
    if (!out || n == 0) {
        return;
    }
    if (r.tempC == kNoTemp) {
        snprintf(out, n, "--");
    } else if (r.dewC == kNoTemp) {
        snprintf(out, n, "%d\xC2\xB0" "C", r.tempC);
    } else {
        snprintf(out, n, "%d\xC2\xB0" "C, dew %d\xC2\xB0" "C", r.tempC, r.dewC);
    }
}

bool pickNewest(const char* body, size_t len, const char* station, char* out, size_t cap)
{
    if (!body || !station || !out || cap == 0 || strlen(station) != 4) {
        return false;
    }
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        while (i < len && body[i] != '\n') {
            i++;
        }
        size_t e = i;
        i++;   // skip '\n'
        while (s < e && (body[s] == ' ' || body[s] == '\r')) {
            s++;
        }
        while (e > s && (body[e - 1] == '\r' || body[e - 1] == ' ')) {
            e--;
        }
        if (e == s) {
            continue;
        }
        // station: first token, or the second after METAR/SPECI
        size_t p = s;
        if (e - p >= 6 && (memcmp(body + p, "METAR ", 6) == 0 || memcmp(body + p, "SPECI ", 6) == 0)) {
            p += 6;
        }
        if (e - p < 4) {
            continue;
        }
        bool match = (e - p == 4 || body[p + 4] == ' ');
        for (int k = 0; k < 4 && match; k++) {
            char a = body[p + k], b = station[k];
            if (a >= 'a' && a <= 'z') a = char(a - 32);
            if (b >= 'a' && b <= 'z') b = char(b - 32);
            match = a == b;
        }
        if (!match) {
            continue;
        }
        size_t n = e - s;
        if (n > kMaxRaw) n = kMaxRaw;
        if (n > cap - 1) n = cap - 1;
        memcpy(out, body + s, n);
        out[n] = 0;
        return true;
    }
    return false;
}

} // namespace Metar
} // namespace core
