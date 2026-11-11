#include "core/SettingDefs.h"
#include <cstdio>
#include <cstring>
#include "core/Zones.h"

namespace core {

static const SettingDef kDefs[] = {
    //  key        type                def   min   max  step secret editable
    {"bright", SettingType::U8, 3, 0, 4, 0, false, true},        // 5 steps → 20..100 %
    {"overlay", SettingType::Bool, 0, 0, 1, 0, false, true},     // diagnostics overlay
    {"loglvl", SettingType::U8, 2, 0, 3, 0, false, true},        // 0 E,1 W,2 I,3 D
    {"simfail", SettingType::U16, 0, 0, 0xFF, 0, false, false},  // simulated failures (serial only)
    // S2 (schema 3)
    {"wifissid", SettingType::Str, 0, 1, 32, 0, false, false},   // set via setup portal / `ad wifi set`
    {"wifipsk", SettingType::Str, 0, 8, 63, 0, true, false},     // WPA2/3-Personal; secret
    {"tzid", SettingType::U16, int32_t(Zones::defaultIndex()), 0, int32_t(Zones::count()) - 1, 0, false, true},
    {"syncmin", SettingType::U16, 60, 15, 1440, 0, false, true}, // sync interval (minutes)
    {"lastsync", SettingType::I32, 0, 0, 0x7FFFFFFF, 0, false, false}, // UTC s of the last successful sync
    // S3 (schema 4)
    {"wxstn", SettingType::Str, 0, 4, 4, 0, false, true, "LTAC"},   // METAR station (ICAO)
};
static_assert(sizeof(kDefs) / sizeof(kDefs[0]) == size_t(SettingKey::Count), "registry/enum mismatch");

size_t settingCount() { return size_t(SettingKey::Count); }

const SettingDef& settingDef(SettingKey k)
{
    size_t i = size_t(k);
    return kDefs[i < settingCount() ? i : 0];
}

bool validateSetting(SettingKey k, int32_t v)
{
    const SettingDef& d = settingDef(k);
    if (v < d.min || v > d.max) {
        return false;
    }
    return d.step <= 0 || (v - d.min) % d.step == 0;
}

int32_t sanitizeSetting(SettingKey k, int32_t stored, bool present)
{
    if (!present || !validateSetting(k, stored)) {
        return settingDef(k).def;
    }
    return stored;
}

SchemaAction schemaAction(uint16_t stored, uint16_t current, bool present)
{
    if (!present) {
        return SchemaAction::FirstBoot;
    }
    if (stored == 0 || stored > current) {
        return SchemaAction::ResetNewer;
    }
    if (stored < current) {
        return SchemaAction::Migrate;
    }
    return SchemaAction::Load;
}

int findSettingKey(const char* name)
{
    if (!name) {
        return -1;
    }
    for (size_t i = 0; i < settingCount(); i++) {
        if (strcmp(kDefs[i].key, name) == 0) {
            return int(i);
        }
    }
    return -1;
}

void formatSettingForOutput(const SettingDef& d, int32_t value, char* out, size_t n)
{
    if (!out || n == 0) {
        return;
    }
    if (d.secret) {
        snprintf(out, n, "***");
    } else {
        snprintf(out, n, "%ld", (long)value);
    }
}

bool validateString(SettingKey k, const char* v)
{
    const SettingDef& d = settingDef(k);
    if (d.type != SettingType::Str || !v) {
        return false;
    }
    const size_t n = strlen(v);
    if (n < size_t(d.min) || n > size_t(d.max)) {
        return false;
    }
    if (k == SettingKey::WxStation) {
        for (size_t i = 0; i < n; i++) {
            const char c = v[i];
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
                return false;
            }
        }
    }
    return true;
}

void normalizeString(SettingKey k, char* v)
{
    if (!v || k != SettingKey::WxStation) {
        return;
    }
    for (; *v; v++) {
        if (*v >= 'a' && *v <= 'z') {
            *v = char(*v - 32);
        }
    }
}

uint16_t migrateTzoffToTzid(int32_t tzoffMin) { return uint16_t(Zones::bestMatch(int16_t(tzoffMin))); }

} // namespace core
