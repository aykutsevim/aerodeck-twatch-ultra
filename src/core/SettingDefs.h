// AeroDeck core: settings registry, validation and schema decisions (data-model §1). Host-tested.
#pragma once
#include <cstddef>
#include <cstdint>

namespace core {

enum class SettingType : uint8_t { Bool, U8, U16, I32 };
enum class SettingKey : uint8_t { Bright, Overlay, LogLvl, SimFail, Count };

struct SettingDef {
    const char* key;   // NVS key, ≤15 chars
    SettingType type;
    int32_t def;
    int32_t min;
    int32_t max;
    bool secret;       // never logged or displayed (FR-013)
    bool editable;     // shown on the Settings page
};

constexpr uint16_t kSchemaVersion = 1;
constexpr const char* kSchemaKey = "schema";
constexpr const char* kNamespace = "aerodeck";

enum class SchemaAction : uint8_t { FirstBoot, Load, Migrate, ResetNewer };

size_t settingCount();
const SettingDef& settingDef(SettingKey k);
bool validateSetting(SettingKey k, int32_t v);
int32_t sanitizeSetting(SettingKey k, int32_t stored, bool present);
SchemaAction schemaAction(uint16_t stored, uint16_t current, bool present);
int findSettingKey(const char* name);   // SettingKey index or -1
// Writes the value as text, or "***" for secret settings.
void formatSettingForOutput(const SettingDef& d, int32_t value, char* out, size_t n);

} // namespace core
