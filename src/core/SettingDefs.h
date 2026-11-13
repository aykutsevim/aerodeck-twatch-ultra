// AeroDeck core: settings registry, validation and schema decisions (data-model §1). Host-tested.
#pragma once
#include <cstddef>
#include <cstdint>

namespace core {

enum class SettingType : uint8_t { Bool, U8, U16, I32, Str };   // Str: min/max = length limits
enum class SettingKey : uint8_t {
    Bright, Overlay, LogLvl, SimFail,          // S0
    WifiSsid, WifiPsk, TzId, SyncMin, LastSync, // S2 (S1's TzOff replaced by TzId)
    WxStation,                                  // S3
    HapticLvl, Quiet,                           // S4
    Volume, Chime, QhStart, QhEnd,              // S5
    Stride,                                     // S6
    Nvg, ScrTo, Raise, AttRef,                  // S7
    Count
};

struct SettingDef {
    const char* key;   // NVS key, ≤15 chars
    SettingType type;
    int32_t def;
    int32_t min;
    int32_t max;
    int32_t step;      // 0 = any value in range; otherwise (v - min) % step == 0
    bool secret;       // never logged or displayed (FR-013)
    bool editable;     // shown on the Settings page
    const char* sdef = nullptr;   // string default (Str type); nullptr = empty
};

constexpr uint16_t kSchemaVersion = 8;   // v3 (S2): wifi*, tzid, syncmin, lastsync; tzoff removed. v4 (S3): wxstn. v5 (S4): hapticlvl, quiet. v6 (S5): volume, chime, qhstart, qhend. v7 (S6): stride. v8 (S7): nvg, scrto, raise, attref
constexpr const char* kLegacyTzoffKey = "tzoff";   // S1 key, migrated to tzid
constexpr const char* kSchemaKey = "schema";
constexpr const char* kNamespace = "aerodeck";

enum class SchemaAction : uint8_t { FirstBoot, Load, Migrate, ResetNewer };

size_t settingCount();
const SettingDef& settingDef(SettingKey k);
bool validateSetting(SettingKey k, int32_t v);
int32_t sanitizeSetting(SettingKey k, int32_t stored, bool present);
SchemaAction schemaAction(uint16_t stored, uint16_t current, bool present);
int findSettingKey(const char* name);   // SettingKey index or -1
// String settings: length within [min, max] of the def (ssid 1..32, psk 8..63).
// wxstn: exactly 4 letters A-Z, case-insensitive.
bool validateString(SettingKey k, const char* v);
// Canonical form before storing (wxstn → uppercase); other keys unchanged.
void normalizeString(SettingKey k, char* v);
// Migration 2→3: the S1 fixed UTC offset (minutes) → index of an equivalent zone (Zones::bestMatch).
uint16_t migrateTzoffToTzid(int32_t tzoffMin);
// Writes the value as text, or "***" for secret settings.
void formatSettingForOutput(const SettingDef& d, int32_t value, char* out, size_t n);

} // namespace core
