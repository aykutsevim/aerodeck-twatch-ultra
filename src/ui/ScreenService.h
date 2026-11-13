// AeroDeck screen power (S7, FR-001…005, research R4/R5). UI task only, except status() (any task).
// Drives core::ScreenPolicy: idle timeout, raise-to-wake, button/alert wake, touch suppression after a wake.
// While the screen is off the UI loop must not call lv_timer_handler() (isOn() == false).
#pragma once
#include <cstdint>
#include "core/Alerts.h"
#include "services/Snapshot.h"

struct ScreenSnapshot {
    bool on;
    uint8_t cause;              // core::ScreenPolicy::Cause
    uint32_t idleMs;
    uint8_t timeoutIdx;
    bool raiseEnabled;
    bool raiseAvailable;
    bool nvg;
    uint8_t brightnessPct;      // as applied (10 in NVG)
    uint32_t lastWakeLatencyMs; // wake decision → first rendered frame
    uint32_t wakes;
    uint32_t raises;            // raise events detected (incl. while on)
    uint32_t seq;
};

namespace ScreenService {
void begin();                                 // after the UI is up; installs the gravity tap and touch filter
void configure(uint8_t timeoutIdx, bool raiseEnabled);
void setApplied(bool nvg, uint8_t brightnessPct);   // for the snapshot
void tick(uint32_t nowMs);                    // every UI loop iteration (works every 50 ms)
bool onButton();                              // true = consumed (the press only woke the screen)
void onAlert(core::AlertLevel lvl);           // new/re-armed Caution/Warning
void command(uint8_t cmd);                    // 0 off, 1 on, 2 raise (console test hook, via ScreenCommand)
void forceOff();                              // eject quiesce: display off now, no policy
bool isOn();
const Snapshot<ScreenSnapshot>& status();
void registerCommands();                      // `ad screen [on|off|raise]`, `ad nvg on|off` (svc task handlers)
} // namespace ScreenService
