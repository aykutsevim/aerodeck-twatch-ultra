// AeroDeck alert manager (S4): the single path for WARNING / CAUTION / ADVISORY alerts (replaces S0 Cautions).
// raise/clear/ack are callable from any task; haptic patterns and the WARNING repeat run on the svc tick.
// Logic: core::AlertSet + core::Alerts table. UI reads Snapshot<AlertSnapshot>.
#pragma once
#include <cstdint>
#include "core/AlertSet.h"
#include "services/Snapshot.h"

struct AlertSnapshot {
    uint8_t n;                 // items used (<= 16), master first
    core::Alert items[16];
    uint8_t total;             // active alerts (may exceed n)
    bool anyUnacked;
    uint32_t seq;
};

namespace AlertManager {
void begin();                                                  // before any raise; registers `ad alert`
void tick(uint32_t nowMs);                                     // svc task: patterns, WARNING repeat
void raise(core::AlertId id);                                  // level and text from the table
void raise(core::AlertId id, core::AlertLevel level, const char* text);
void raiseAcked(core::AlertId id);                             // raise already acknowledged (no pattern)
void clear(core::AlertId id);
uint8_t ackAll();                                              // tap on the band / `ad alert ack`
const Snapshot<AlertSnapshot>& status();
} // namespace AlertManager
