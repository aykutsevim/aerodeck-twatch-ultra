#include "core/SettingDefs.h"
#include <cstdio>
#include <cstring>

namespace core {

static const SettingDef kDefs[] = {
    //  key        type                def   min   max  step secret editable
    {"bright", SettingType::U8, 3, 0, 4, 0, false, true},        // 5 steps → 20..100 %
    {"overlay", SettingType::Bool, 0, 0, 1, 0, false, true},     // diagnostics overlay
    {"loglvl", SettingType::U8, 2, 0, 3, 0, false, true},        // 0 E,1 W,2 I,3 D
    {"simfail", SettingType::U16, 0, 0, 0xFF, 0, false, false},  // simulated failures (serial only)
    {"tzoff", SettingType::I32, 180, -720, 840, 30, false, true}, // S1: local = UTC + tzoff minutes
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

} // namespace core
