// AeroDeck settings: RAM cache over NVS (Preferences). Writes are executed on the svc task so the UI
// never blocks on flash (FR-018). Contract: contracts/internal_interfaces.md, data-model §1.
#pragma once
#include <cstddef>
#include <cstdint>
#include "core/SettingDefs.h"

namespace SettingsStore {

enum class SetResult : uint8_t { Ok, OutOfRange, NotEditable };

enum class BootStatus : uint8_t { Loaded, FirstBoot, Migrated, ResetNewer, StorageError };

BootStatus begin();                       // synchronous, call before SvcTask::begin
int32_t get(core::SettingKey k);
inline bool getBool(core::SettingKey k) { return get(k) != 0; }
// Validates, updates the cache, publishes SettingsChanged and queues the NVS write.
// `internal` bypasses the editable check (used by `ad simfail`).
SetResult set(core::SettingKey k, int32_t v, bool internal = false);
void restoreDefaults();
// String settings (S2): Wi-Fi credentials. getStr copies into out (always NUL-terminated).
void getStr(core::SettingKey k, char* out, size_t n);
SetResult setStr(core::SettingKey k, const char* v);   // validates length; persists on the svc task
void clearStr(core::SettingKey k);                       // e.g. "forget network"
void registerCommands();                  // `ad set ...`
void tick(uint32_t nowMs);                // svc task: performs queued writes

} // namespace SettingsStore
