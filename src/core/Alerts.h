// AeroDeck core: alert levels, ids and the alert table (S4 data-model §1). Pure, host-tested.
// Ids keep the S0 CautionId numbering; later slices (S5 voice, S8 timer, S9 radio) extend the table.
#pragma once
#include <cstddef>
#include <cstdint>
#include "core/HapticPolicy.h"
#include "core/Types.h"

namespace core {

enum class AlertLevel : uint8_t { None = 0, Advisory = 1, Caution = 2, Warning = 3 };

enum class AlertId : uint8_t {
    PeriphFirst = 0,   // PeriphFirst + Periph index → "<NAME> FAIL" (0..12)
    SettingsReset = 32,
    SettingsStorageError = 33,
    SafeMode = 34,
    ClockNotSet = 35,
    // 36: S1 LOW FUEL, retired (replaced by Stall)
    TimeUnsync = 37,
    Stall = 40,
    PullUp = 41,
    Test1 = 60,
    Test2 = 61,
    Test3 = 62,
};

struct AlertDef {
    AlertId id;
    AlertLevel level;
    Pattern pattern;
    uint16_t repeatMs;     // 0 = play once
    uint8_t voiceClip;     // 0 = none (filled by S5)
    const char* text;      // nullptr for peripheral ids (built by alertText)
    const char* name;      // serial name (lowercase)
};

const AlertDef* alertDef(AlertId id);   // nullptr when the id is not in the table
void alertText(AlertId id, char* out, size_t n);   // default message ("IMU FAIL", "STALL", ...)
bool alertIdFromName(const char* name, AlertId& out);   // table name, peripheral name, or a number
bool levelFromLetter(const char* s, AlertLevel& out);   // W / C / A (case-insensitive)
const char* levelName(AlertLevel l);                     // "WARNING" "CAUTION" "ADVISORY"
Pattern patternForLevel(AlertLevel l);

} // namespace core
