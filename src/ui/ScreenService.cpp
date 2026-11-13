#include "ui/ScreenService.h"
#include <Arduino.h>
#include <lvgl.h>
#include "board/IBoard.h"
#include "core/RaiseDetector.h"
#include "core/ScreenPolicy.h"
#include "services/AttitudeService.h"
#include "services/EventBus.h"
#include "services/Log.h"
#include "services/SerialConsole.h"
#include "services/SettingsStore.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "ui/NetSetupOverlay.h"

namespace ScreenService {

using core::ScreenPolicy;

static ScreenPolicy s_policy;
static core::RaiseDetector s_raise;
static Snapshot<ScreenSnapshot> s_snap;
static volatile bool s_raisePending;   // set by the gravity tap (UI task), consumed by tick
static volatile uint32_t s_raises;
static bool s_touchDown;
static bool s_nvg;
static uint8_t s_brightPct = 80;
static uint32_t s_lastTickMs;
static uint32_t s_wakeAtMs;
static bool s_waitFirstFrame;
static uint32_t s_wakeLatencyMs;
static uint32_t s_wakes;
static uint32_t s_seq;
static lv_indev_read_cb_t s_origRead;
static volatile bool s_trace;   // `ad screen trace on`: gravity at 20 Hz for tuning (T020)
static uint8_t s_traceDiv;

// UI task (AttitudeService gravity callback): O(1).
static void gravityTap(float x, float y, float z, uint32_t ms)
{
    if (s_trace && ++s_traceDiv >= 5) {
        s_traceDiv = 0;
        const float n = sqrtf(x * x + y * y + z * z);
        if (n > 1e-6f) {
            LOGI("grav", "%lu %.2f %.2f %.2f", (unsigned long)ms, x / n, y / n, z / n);
        }
    }
    if (s_raise.feed(x, y, z, ms)) {
        s_raisePending = true;
        s_raises = s_raises + 1;
    }
}

// Touch filter: while a wake's touch suppression is active, presses are reported as released (FR-002).
static void filteredRead(lv_indev_t* in, lv_indev_data_t* data)
{
    if (s_origRead) {
        s_origRead(in, data);
    }
    s_touchDown = data->state == LV_INDEV_STATE_PRESSED;
    if (s_policy.suppressTouch() || !s_policy.on()) {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void onRenderReady(lv_event_t*)
{
    if (s_waitFirstFrame) {
        s_waitFirstFrame = false;
        s_wakeLatencyMs = millis() - s_wakeAtMs;
        LOGI("scr", "on (%s) %lu ms", ScreenPolicy::causeName(s_policy.cause()), (unsigned long)s_wakeLatencyMs);
    }
}

static bool raiseAvailable()
{
    return AttitudeService::att().read().status == AttStatus::Ok;
}

static void publish(uint32_t now, uint32_t inactive)
{
    ScreenSnapshot s;
    s.on = s_policy.on();
    s.cause = uint8_t(s_policy.cause());
    s.idleMs = s_policy.idleMs(now, inactive);
    s.timeoutIdx = s_policy.timeoutIdx();
    s.raiseEnabled = s_policy.raiseEnabled();
    s.raiseAvailable = s_policy.raiseAvailable();
    s.nvg = s_nvg;
    s.brightnessPct = s_brightPct;
    s.lastWakeLatencyMs = s_wakeLatencyMs;
    s.wakes = s_wakes;
    s.raises = s_raises;
    s.seq = ++s_seq;
    s_snap.write(s);
}

void begin()
{
    s_policy.begin(millis());
    AttitudeService::setGravityTap(gravityTap);
    for (lv_indev_t* in = lv_indev_get_next(nullptr); in; in = lv_indev_get_next(in)) {
        if (lv_indev_get_type(in) == LV_INDEV_TYPE_POINTER) {
            s_origRead = lv_indev_get_read_cb(in);
            lv_indev_set_read_cb(in, filteredRead);
            break;
        }
    }
    lv_display_add_event_cb(lv_display_get_default(), onRenderReady, LV_EVENT_RENDER_READY, nullptr);
    publish(millis(), 0);
}

void configure(uint8_t timeoutIdx, bool raiseEnabled)
{
    s_policy.configure(timeoutIdx, raiseEnabled, raiseAvailable());
}

void setApplied(bool nvg, uint8_t brightnessPct)
{
    s_nvg = nvg;
    s_brightPct = brightnessPct;
}

static void turnOn(uint32_t now)
{
    board().setDisplayOn(true);
    lv_obj_invalidate(lv_screen_active());
    lv_obj_invalidate(lv_layer_top());
    lv_display_trigger_activity(nullptr);
    s_wakeAtMs = now;
    s_waitFirstFrame = true;
    s_wakes++;
    EventBus::publish(EventType::ScreenChanged, 1, uint32_t(s_policy.cause()));
}

static void turnOff(uint32_t idleMs)
{
    board().setDisplayOn(false);
    LOGI("scr", "off (idle %lu s)", (unsigned long)(idleMs / 1000));
    EventBus::publish(EventType::ScreenChanged, 0, 0);
}

void tick(uint32_t now)
{
    if (now - s_lastTickMs < 50) {
        return;
    }
    s_lastTickMs = now;
    // Raise availability can change after boot (the IMU starts in the deferred probe).
    s_policy.configure(s_policy.timeoutIdx(), s_policy.raiseEnabled(), raiseAvailable());
    if (s_raisePending) {
        s_raisePending = false;
        s_policy.onRaise(now);
    }
    const uint32_t inactive = lv_display_get_inactive_time(nullptr);
    const uint32_t idle = s_policy.idleMs(now, inactive);
    switch (s_policy.update(now, inactive, NetSetupOverlay::isShown(), s_touchDown)) {
    case ScreenPolicy::Action::TurnOn: turnOn(now); break;
    case ScreenPolicy::Action::TurnOff: turnOff(idle); break;
    default: break;
    }
    publish(now, inactive);
}

bool onButton()
{
    return s_policy.onButton(millis());
}

void onAlert(core::AlertLevel lvl)
{
    s_policy.onAlert(lvl, millis());
}

void command(uint8_t cmd)
{
    const uint32_t now = millis();
    if (cmd == 2) {
        s_raises = s_raises + 1;
        s_policy.onRaise(now);
    } else {
        s_policy.command(cmd == 1, now);
    }
}

void forceOff()
{
    if (s_policy.on()) {
        s_policy.command(false, millis());
        s_policy.update(millis(), 0, false, false);
    }
    board().setDisplayOn(false);
}

bool isOn() { return s_policy.on(); }

const Snapshot<ScreenSnapshot>& status() { return s_snap; }

// Console handlers run on the svc task: read the snapshot, post ScreenCommand, or change a setting.
static void cmdScreen(int argc, const char* const* argv, core::Reply& r)
{
    if (argc >= 2 && strcmp(argv[0], "trace") == 0) {
        s_trace = strcmp(argv[1], "on") == 0;
        r.ok();
        return;
    }
    if (argc >= 1) {
        uint32_t c = strcmp(argv[0], "off") == 0 ? 0 : strcmp(argv[0], "on") == 0 ? 1
                   : strcmp(argv[0], "raise") == 0 ? 2 : 99;
        if (c == 99) {
            r.err("expected on|off|raise");
            return;
        }
        EventBus::publish(EventType::ScreenCommand, c);
        r.ok();
        return;
    }
    static const char* kTo[] = {"10s", "15s", "30s", "1min", "5min", "never"};
    ScreenSnapshot s = s_snap.read();
    char b[200];
    snprintf(b, sizeof b,
             "on=%d cause=%s idle=%.1fs timeout=%s raise=%s(%s) nvg=%d bright=%u%% wake_ms=%lu wakes=%lu raises=%lu",
             s.on ? 1 : 0, ScreenPolicy::causeName(ScreenPolicy::Cause(s.cause)), s.idleMs / 1000.0f,
             kTo[s.timeoutIdx < 6 ? s.timeoutIdx : 1], s.raiseEnabled ? "on" : "off",
             s.raiseAvailable ? "avail" : "n/a", s.nvg ? 1 : 0, unsigned(s.brightnessPct),
             (unsigned long)s.lastWakeLatencyMs, (unsigned long)s.wakes, (unsigned long)s.raises);
    r.ok();
    r.line(b);
}

static void cmdNvg(int, const char* const* argv, core::Reply& r)
{
    int v = strcmp(argv[0], "on") == 0 ? 1 : strcmp(argv[0], "off") == 0 ? 0 : -1;
    if (v < 0) {
        r.err("expected on|off");
        return;
    }
    SettingsStore::set(core::SettingKey::Nvg, v);   // persisted; applied on the UI task via SettingsChanged
    char b[16];
    snprintf(b, sizeof b, "nvg=%d", v);
    r.ok();
    r.line(b);
}

void registerCommands()
{
    SerialConsole::add("screen", 0, cmdScreen);
    SerialConsole::add("nvg", 1, cmdNvg);
}

} // namespace ScreenService
