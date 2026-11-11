// AeroDeck core: METAR decoder, ceiling, flight category, observation time and staleness (S3 data-model §1,
// research R5/R6). Pure C++, no heap, host-tested (test/test_metar). Input is untrusted network data (FR-020).
#pragma once
#include <climits>
#include <cstddef>
#include <cstdint>

namespace core {
namespace Metar {

constexpr size_t kMaxRaw = 255;            // longer input is truncated before decoding
constexpr int16_t kMissing = -1;
constexpr int16_t kWindVrb = -2;           // windDir for VRB
constexpr int8_t kNoTemp = -128;
constexpr int32_t kNoCeiling = INT32_MAX;  // no BKN/OVC/VV layer: unlimited
constexpr int32_t kCeilingUnknown = -2;    // not enough cloud information
constexpr int kMaxClouds = 6;
constexpr int64_t kStaleAfterS = 90 * 60;  // FR-011

enum class Cover : uint8_t { Few, Sct, Bkn, Ovc, Vv, Unknown };
enum class NoCloud : uint8_t { Unset, Nsc, Ncd, Skc, Clr };
enum class Category : uint8_t { Unknown, Vfr, Mvfr, Ifr, Lifr };

struct Cloud {
    Cover cover;
    int16_t baseFt;   // -1 = unknown (///)
    bool cb;          // CB or TCU (convective)
    bool tcu;         // TCU (display)
};

struct Report {
    char station[5];
    int8_t day, hour, minute;   // -1 = missing
    bool nil;
    int16_t windDir;            // 0..360, kWindVrb, -1 missing
    int16_t windSpdKt;          // -1 missing; 0 with dir 0 = calm
    int16_t windGustKt;
    int16_t windVarFrom, windVarTo;
    bool cavok;
    int32_t visM;               // prevailing visibility in metres, -1 missing (9999 → 10000)
    bool visIsSm;               // reported in statute miles
    uint16_t visSm16;           // statute miles × 16 (when visIsSm)
    int8_t visQual;             // -1 "less than" (M), +1 "more than" (P), 0 exact
    char visText[12];           // display form: "10 km+", "4000 m", "1 1/2 SM", "6+ SM", "<1/4 SM", "CAVOK"
    Cloud clouds[kMaxClouds];
    uint8_t nClouds;
    NoCloud noCloud;
    int8_t tempC, dewC;         // kNoTemp = missing
    int16_t qnhHpa;             // -1 missing
    bool qnhWasInHg;
};

// Decodes `raw` (truncated to kMaxRaw). Returns true when a station and a DDHHMMZ time were found.
// Decoding stops at RMK, NOSIG, BECMG, TEMPO. Out-of-range values are treated as missing.
bool decode(const char* raw, Report& out);

int32_t ceilingFt(const Report& r);       // lowest BKN/OVC/VV, kNoCeiling, or kCeilingUnknown
Category category(const Report& r);       // spec FR-007 (incl. the unknown rule)
const char* categoryName(Category c);     // "VFR" "MVFR" "IFR" "LIFR" "--"

// Latest UTC instant with this day/hour/minute that is <= nowUtc + 10 min (current or earlier months).
// 0 when nowUtc <= 0 or the fields are missing/invalid.
int64_t resolveObsTime(int day, int hour, int minute, int64_t nowUtc);
// True when the observation is unknown, the clock is unknown, or the report is more than 90 min old.
bool isStale(int64_t obsUtc, int64_t nowUtc);

// "270° 15G25 kt", "VRB 03 kt", "VRB 250-310° 06 kt", "CALM", "--" (UTF-8 degree sign).
void formatWind(const Report& r, char* out, size_t n);
// "FEW016 BKN030" (up to 3 known-cover layers), "VV002", "CAVOK", "NSC"/"NCD"/"SKC"/"CLR", "--".
void formatClouds(const Report& r, char* out, size_t n);

// Plain-language clouds for the watch: "Scattered 3,500 ft, broken 18,000 ft", "Few 1,600 ft, broken 3,000 ft (CB)",
// "Sky obscured, vertical visibility 200 ft", "Sky clear", "No significant cloud", "No cloud detected",
// "CAVOK: no cloud below 5,000 ft", "--". Up to 3 layers.
void formatCloudsText(const Report& r, char* out, size_t n);
// "9°C, dew 4°C", "-5°C, dew -10°C", "9°C" (dew missing), "--".
void formatTemp(const Report& r, char* out, size_t n);

// First line of a provider body whose station matches (case-insensitive), CR/LF trimmed, capped at kMaxRaw.
bool pickNewest(const char* body, size_t len, const char* station, char* out, size_t cap);

} // namespace Metar
} // namespace core
