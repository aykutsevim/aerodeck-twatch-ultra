#include "services/AlertManager.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include "services/AudioService.h"
#include "services/EventBus.h"
#include "services/HapticService.h"
#include "services/Log.h"
#include "services/SerialConsole.h"

using core::AlertId;
using core::AlertLevel;
using core::RaiseResult;

namespace AlertManager {

namespace {

core::AlertSet s_set;
Snapshot<AlertSnapshot> s_snap;
AlertSnapshot s_tmp;                       // built under the lock
portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
uint32_t s_seq;
volatile uint8_t s_pendingPattern;         // core::Pattern to play on the next tick (highest wins)
uint32_t s_lastShakerMs;
bool s_shakerArmed;                        // an unacked WARNING exists (repeat runs)
uint8_t s_shakerCycles;                    // S5: shaker repeats since the last (re)start; callout every 3rd
constexpr uint8_t kCalloutEveryCycles = 3; // 3 x 2 s = ~6 s (FR-006)

void publishLocked()   // s_lock held
{
    s_tmp.n = uint8_t(s_set.sorted(s_tmp.items, 16));
    s_tmp.total = uint8_t(s_set.count());
    s_tmp.anyUnacked = s_set.anyUnacked();
    s_tmp.seq = ++s_seq;
    s_snap.write(s_tmp);
}

// S5: the voice callout for a raise / re-arm / repeat (FR-005, FR-006). Non-blocking: only queues.
void callout(AlertId id, AlertLevel level)
{
    const core::Clip c = core::calloutFor(id, level);
    if (c == core::Clip::None) {
        return;
    }
    AudioService::play(c, level == AlertLevel::Warning ? core::SoundPrio::Warning : core::SoundPrio::Caution);
}

void requestPattern(core::Pattern p)
{
    if (uint8_t(p) > s_pendingPattern) {
        s_pendingPattern = uint8_t(p);
    }
}

RaiseResult doRaise(AlertId id, AlertLevel level, const char* text, bool acked)
{
    char t[25];
    if (text && *text) {
        snprintf(t, sizeof t, "%s", text);
    } else {
        core::alertText(id, t, sizeof t);
    }
    portENTER_CRITICAL(&s_lock);
    const RaiseResult r = s_set.raise(id, level, t);
    if (acked) {
        s_set.ack(id);
    }
    publishLocked();
    portEXIT_CRITICAL(&s_lock);
    switch (r) {
    case RaiseResult::Added:
    case RaiseResult::Rearmed:
        LOGW("alert", "raise %s %s id=%u%s", core::levelName(level), t, unsigned(id), acked ? " (acked)" : "");
        if (!acked) {
            const core::AlertDef* d = core::alertDef(id);
            requestPattern(level == AlertLevel::Warning ? core::Pattern::StickShaker
                           : d && d->level == level  ? d->pattern
                                                     : core::patternForLevel(level));
            callout(id, level);
        }
        EventBus::publish(EventType::AlertRaised, uint32_t(id));
        break;
    case RaiseResult::Updated:
        EventBus::publish(EventType::AlertRaised, uint32_t(id));
        break;
    case RaiseResult::Full:
        LOGE("alert", "alert set full; dropped id=%u", unsigned(id));
        break;
    case RaiseResult::Same:
        break;
    }
    return r;
}

// ---- serial commands (contracts/serial_commands.md) ----

void cmdList(int, const char* const*, core::Reply& r)
{
    AlertSnapshot s = s_snap.read();
    char b[64];
    int unacked = 0;
    for (uint8_t i = 0; i < s.n; i++) {
        unacked += s.items[i].acked ? 0 : 1;
    }
    r.ok();
    snprintf(b, sizeof b, "n=%u unacked=%d", unsigned(s.total), unacked);
    r.line(b);
    for (uint8_t i = 0; i < s.n; i++) {
        snprintf(b, sizeof b, "%s %s id=%u acked=%d", core::levelName(s.items[i].level), s.items[i].text,
                 unsigned(s.items[i].id), s.items[i].acked ? 1 : 0);
        r.line(b);
    }
}

void cmdRaise(int argc, const char* const* argv, core::Reply& r)
{
    AlertId id;
    if (!core::alertIdFromName(argv[0], id)) {
        r.err("unknown id");
        return;
    }
    const core::AlertDef* d = core::alertDef(id);
    AlertLevel level = d ? d->level : AlertLevel::Caution;
    const char* text = nullptr;
    for (int i = 1; i < argc; i++) {
        AlertLevel l;
        if (core::levelFromLetter(argv[i], l)) {
            level = l;
        } else {
            text = argv[i];
        }
    }
    if (doRaise(id, level, text, false) == RaiseResult::Full) {
        r.err("alert set full");
        return;
    }
    r.ok();
}

void cmdClear(int, const char* const* argv, core::Reply& r)
{
    AlertId id;
    if (!core::alertIdFromName(argv[0], id)) {
        r.err("unknown id");
        return;
    }
    bool had;
    portENTER_CRITICAL(&s_lock);
    had = s_set.contains(id);
    portEXIT_CRITICAL(&s_lock);
    if (!had) {
        r.err("not active");
        return;
    }
    clear(id);
    r.ok();
}

void cmdAck(int, const char* const*, core::Reply& r)
{
    char b[24];
    snprintf(b, sizeof b, "OK acked=%u", unsigned(ackAll()));
    r.line(b);
}

} // namespace

void begin()
{
    portENTER_CRITICAL(&s_lock);
    publishLocked();
    portEXIT_CRITICAL(&s_lock);
    SerialConsole::add("alert", 0, cmdList);
    SerialConsole::add("alert raise", 1, cmdRaise);
    SerialConsole::add("alert clear", 1, cmdClear);
    SerialConsole::add("alert ack", 0, cmdAck);
}

void raise(AlertId id)
{
    const core::AlertDef* d = core::alertDef(id);
    doRaise(id, d ? d->level : AlertLevel::Caution, nullptr, false);
}

void raise(AlertId id, AlertLevel level, const char* text) { doRaise(id, level, text, false); }

void raiseAcked(AlertId id)
{
    const core::AlertDef* d = core::alertDef(id);
    doRaise(id, d ? d->level : AlertLevel::Caution, nullptr, true);
}

void clear(AlertId id)
{
    portENTER_CRITICAL(&s_lock);
    const bool removed = s_set.clear(id);
    if (removed) {
        publishLocked();
    }
    portEXIT_CRITICAL(&s_lock);
    if (removed) {
        LOGI("alert", "clear id=%u", unsigned(id));
        EventBus::publish(EventType::AlertCleared, uint32_t(id));
    }
}

uint8_t ackAll()
{
    portENTER_CRITICAL(&s_lock);
    const uint8_t n = s_set.ackAll();
    if (n) {
        publishLocked();
    }
    portEXIT_CRITICAL(&s_lock);
    if (n) {
        s_pendingPattern = 0;
        HapticService::stopWarning();   // FR-007: repeating vibration stops now
        AudioService::stopAlerts();     // S5 FR-007: callout in progress cut, repeats cancelled
        LOGI("alert", "ack %u", unsigned(n));
        EventBus::publish(EventType::AlertAcked, n);
    }
    return n;
}

void tick(uint32_t now)
{
    const uint8_t p = s_pendingPattern;
    s_pendingPattern = 0;
    portENTER_CRITICAL(&s_lock);
    const bool armed = s_set.anyUnackedWarning();
    portEXIT_CRITICAL(&s_lock);
    if (p == uint8_t(core::Pattern::StickShaker) || (armed && !s_shakerArmed)) {
        HapticService::play(core::Pattern::StickShaker);
        s_lastShakerMs = now;
        s_shakerCycles = 0;   // (re)started by a raise / re-arm: the first voice repeat comes ~6 s later
    } else if (p) {
        HapticService::play(core::Pattern(p));
    } else if (armed && now - s_lastShakerMs >= 2000) {
        HapticService::play(core::Pattern::StickShaker);   // repeat until acknowledged (FR-012)
        s_lastShakerMs = now;
        if (++s_shakerCycles >= kCalloutEveryCycles) {
            s_shakerCycles = 0;
            // re-queue the callout of the master (first) unacknowledged WARNING
            const AlertSnapshot snap = s_snap.read();
            for (uint8_t i = 0; i < snap.n; i++) {
                if (!snap.items[i].acked && snap.items[i].level == AlertLevel::Warning) {
                    callout(snap.items[i].id, AlertLevel::Warning);
                    break;
                }
            }
        }
    }
    s_shakerArmed = armed;
}

const Snapshot<AlertSnapshot>& status() { return s_snap; }

} // namespace AlertManager
