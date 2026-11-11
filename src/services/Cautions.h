// AeroDeck S0 minimal cautions (data-model §4). Superseded by AlertManager in S4 (ids kept).
#pragma once
#include <cstddef>
#include <cstdint>

enum class CautionId : uint8_t {
    PeriphFirst = 0,   // PeriphFirst + core::Periph index → "<NAME> FAIL"
    SettingsReset = 32,
    SettingsStorageError,
    SafeMode,
    ClockNotSet,   // S1
    LowFuel,       // S1
};

struct Caution {
    CautionId id;
    char text[25];
};

namespace Cautions {
constexpr size_t kMax = 16;
void raise(CautionId id, const char* text);
void clear(CautionId id);
size_t active(Caution* out, size_t max);   // thread-safe copy
} // namespace Cautions
