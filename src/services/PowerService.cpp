#include "services/PowerService.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "board/IBoard.h"
#include "services/EventBus.h"
#include "services/AlertManager.h"
#include "services/AudioService.h"
#include "services/NetService.h"
#include "services/RecorderService.h"
#include "services/SettingsStore.h"
#include <esp_attr.h>
#include "services/Log.h"
#include "services/SerialConsole.h"
#include "core/BatteryAlarm.h"
#include "core/Fuel.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

using core::Rail;

static uint8_t s_ref[size_t(Rail::Count)];
static SemaphoreHandle_t s_mutex;
static Snapshot<PowerSnapshot> s_snap;
static uint32_t s_lastPollMs;
static core::FuelFlow s_flow;
static core::FuelState s_fuelState;
static volatile int s_simPct = -1;   // `ad fuel sim` override, -1 = off
static volatile int s_simChg = -1;   // S4: `ad fuel sim <pct> chg|nochg` charging override, -1 = off
static core::BatteryAlarm s_battAlarm;   // S4: STALL / PULL UP (FR-012)
static volatile bool s_alarmEnabled;     // S4: only after the watch face appears
static uint32_t s_seq;

// S7: eject, profiling
static QueueHandle_t s_events;
static volatile bool s_ejecting;
static SemaphoreHandle_t s_quiesced;
static PowerService::PreSleepHook s_sleepHooks[4];
static uint8_t s_sleepHookCount;
static volatile bool s_profile;   // `ad pwr log on`
static bool s_screenOn = true;    // from ScreenChanged (services do not read UI state)
struct EjectRecord {              // survives deep sleep (RTC slow memory, not initialised on wake)
    uint32_t magic;
    uint32_t requestToSleepMs;    // EjectRequested handled → deep sleep call
    uint32_t gestureToSleepMs;    // gesture end (event timestamp) → deep sleep call
    uint32_t recSaveMs;
    uint32_t quiesceMs;
    uint32_t wakeInS;
    uint8_t source;               // 0 gesture, 1 console
};
RTC_NOINIT_ATTR static EjectRecord s_ejectRec;
constexpr uint32_t kEjectMagic = 0xE1EC7007;

static const char* railName(Rail r)
{
    switch (r) {
    case Rail::Display: return "display";
    case Rail::Lora: return "lora";
    case Rail::AudioAmp: return "audioamp";
    case Rail::Gnss: return "gnss";
    case Rail::Nfc: return "nfc";
    case Rail::Sensor: return "sensor";
    case Rail::Wifi: return "wifi";
    default: return "?";
    }
}

// Rails that are not PMU outputs (Wi-Fi) are switched by a hook registered by their owning service (S2).
static PowerService::RailHook s_hooks[size_t(Rail::Count)];

static void switchRail(Rail r, bool on)
{
    if (s_hooks[size_t(r)]) {
        s_hooks[size_t(r)](on);
    } else {
        board().setRail(r, on);
    }
}

static void adjust(Rail r, int delta)
{
    if (!s_mutex) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint8_t& c = s_ref[size_t(r)];
    uint8_t before = c;
    if (delta > 0 && c < 255) {
        c++;
    } else if (delta < 0 && c > 0) {
        c--;
    }
    if (before == 0 && c == 1) {
        switchRail(r, true);
        LOGI("pwr", "rail %s ON", railName(r));
    } else if (before == 1 && c == 0) {
        switchRail(r, false);
        LOGI("pwr", "rail %s OFF", railName(r));
    }
    xSemaphoreGive(s_mutex);
}

RailLease::RailLease(Rail r) : rail_(r), held_(true) { adjust(r, +1); }
RailLease::~RailLease()
{
    if (held_) {
        adjust(rail_, -1);
    }
}
RailLease::RailLease(RailLease&& o) noexcept : rail_(o.rail_), held_(o.held_) { o.held_ = false; }
RailLease& RailLease::operator=(RailLease&& o) noexcept
{
    if (this != &o) {
        if (held_) {
            adjust(rail_, -1);
        }
        rail_ = o.rail_;
        held_ = o.held_;
        o.held_ = false;
    }
    return *this;
}

namespace PowerService {

void begin()
{
    s_mutex = xSemaphoreCreateMutex();
    s_quiesced = xSemaphoreCreateBinary();
    s_events = EventBus::subscribe(8);
    // FR-017: no feature needs these in S0, so they start OFF. Display and Sensor stay on (touch/IMU).
    const Rail optional[] = {Rail::Lora, Rail::AudioAmp, Rail::Gnss, Rail::Nfc, Rail::Wifi};
    for (Rail r : optional) {
        switchRail(r, false);
        LOGI("pwr", "rail %s OFF (boot)", railName(r));
    }
}

const Snapshot<PowerSnapshot>& power() { return s_snap; }

void enforceIdleRails()
{
    const Rail optional[] = {Rail::Lora, Rail::AudioAmp, Rail::Gnss, Rail::Nfc, Rail::Wifi};
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (Rail r : optional) {
        if (s_ref[size_t(r)] == 0) {
            switchRail(r, false);
        }
    }
    xSemaphoreGive(s_mutex);
    LOGI("pwr", "idle rails enforced OFF");
}

uint8_t refcount(Rail r) { return s_ref[size_t(r)]; }

void setRailHook(Rail r, RailHook fn) { s_hooks[size_t(r)] = fn; }

static void eject(uint8_t source, uint32_t gestureMs);
static void profileLine(uint32_t nowMs);

void tick(uint32_t nowMs)
{
    Event e;
    while (s_events && xQueueReceive(s_events, &e, 0) == pdTRUE) {
        if (e.type == EventType::EjectRequested) {
            eject(uint8_t(e.a), e.b);
        } else if (e.type == EventType::ScreenChanged) {
            s_screenOn = e.a != 0;
        }
    }
    if (nowMs - s_lastPollMs < 1000) {
        return;
    }
    s_lastPollMs = nowMs;
    PowerSnapshot p{};
    p.valid = board().readPmu(p.pmu);
    p.atMs = nowMs;
    if (p.valid && s_simPct >= 0) {
        p.pmu.battPct = uint8_t(s_simPct);
    }
    if (p.valid && s_simChg >= 0) {
        p.pmu.charging = s_simChg != 0;
    }
    p.fuelFlowPctPerH = NAN;
    if (p.valid) {
        s_flow.addSample(nowMs / 60000, p.pmu.battPct, p.pmu.charging);
        p.fuelFlowPctPerH = s_flow.ratePctPerH();
        p.lowFuel = s_fuelState.update(p.pmu.battPct, p.pmu.charging);   // EICAS gauge colour (S1)
        // S4: the LOW FUEL caution is replaced by STALL / PULL UP warnings with hysteresis (core::BatteryAlarm).
        switch (s_alarmEnabled ? s_battAlarm.update(p.pmu.battPct, p.pmu.charging, true) : core::BattAction::None) {
        case core::BattAction::RaiseStall:
            AlertManager::raise(core::AlertId::Stall);
            break;
        case core::BattAction::RaiseStallAcked:
            AlertManager::clear(core::AlertId::PullUp);
            AlertManager::raiseAcked(core::AlertId::Stall);   // stepping down doesn't nag again
            break;
        case core::BattAction::RaisePullUp:
            AlertManager::clear(core::AlertId::Stall);
            AlertManager::raise(core::AlertId::PullUp);
            break;
        case core::BattAction::ClearAll:
            AlertManager::clear(core::AlertId::Stall);
            AlertManager::clear(core::AlertId::PullUp);
            break;
        case core::BattAction::None:
            break;
        }
    }
    p.flowSamples = uint8_t(s_flow.count());
    p.seq = ++s_seq;
    s_snap.write(p);
    EventBus::publish(EventType::PowerSnapshotUpdated);
    if (s_profile) {
        profileLine(nowMs);
    }
}

// S7 (FR-018, NFR-PWR-004): one line per second. The AXP2101 has no battery current ADC: ma=n/a (research R9).
static void profileLine(uint32_t nowMs)
{
    const PowerSnapshot p = s_snap.read();
    char flow[12];
    if (std::isnan(p.fuelFlowPctPerH)) {
        snprintf(flow, sizeof flow, "n/a");
    } else {
        snprintf(flow, sizeof flow, "%.1f", p.fuelFlowPctPerH);
    }
    char ma[8];
    if (p.pmu.battmA == core::kCurrentUnavailable) {
        snprintf(ma, sizeof ma, "n/a");
    } else {
        snprintf(ma, sizeof ma, "%d", int(p.pmu.battmA));
    }
    LOGI("pwr", "prof t=%lu pct=%u mv=%u chg=%d vbus=%d flow=%s scr=%s nvg=%d "
                "rails=lora:%u,amp:%u,gnss:%u,nfc:%u,wifi:%u ma=%s",
         (unsigned long)(nowMs / 1000), unsigned(p.pmu.battPct), unsigned(p.pmu.battmV), p.pmu.charging ? 1 : 0,
         p.pmu.vbus ? 1 : 0, flow, s_screenOn ? "on" : "off", SettingsStore::getBool(core::SettingKey::Nvg) ? 1 : 0,
         unsigned(s_ref[size_t(Rail::Lora)] > 0), unsigned(s_ref[size_t(Rail::AudioAmp)] > 0),
         unsigned(s_ref[size_t(Rail::Gnss)] > 0), unsigned(s_ref[size_t(Rail::Nfc)] > 0),
         unsigned(s_ref[size_t(Rail::Wifi)] > 0), ma);
}

bool addPreSleepHook(PreSleepHook fn)
{
    if (!fn || s_sleepHookCount >= 4) {
        return false;
    }
    s_sleepHooks[s_sleepHookCount++] = fn;
    return true;
}

bool ejecting() { return s_ejecting; }

void uiQuiesced()
{
    if (s_quiesced) {
        xSemaphoreGive(s_quiesced);
    }
}

// svc task. Service steps get <= 500 ms of wall time (later steps are skipped and logged), then the UI gets
// <= 200 ms to quiesce, then the board sleeps (research R6; FR-008: < 2 s from the gesture).
static void eject(uint8_t source, uint32_t gestureMs)
{
    const uint32_t t0 = millis();
    const uint32_t deadline = t0 + 500;
    LOGW("pwr", "eject (%s)", source == 0 ? "gesture" : "console");
    const bool saved = RecorderService::saveNow("eject");
    const uint32_t recMs = millis() - t0;
    if (int32_t(millis() - deadline) < 0) {
        AudioService::stop(core::SoundPrio::Sample, "eject");
        board().hapticStop();
        if (NetService::status().read().busy == NetBusy::Setup) {
            NetService::stopSetup();
        }
    } else {
        LOGW("pwr", "eject: audio/haptic/net stop skipped (deadline)");
    }
    uint32_t wakeIn = 0;
    uint8_t hooks = 0;
    for (uint8_t i = 0; i < s_sleepHookCount; i++) {
        if (int32_t(millis() - deadline) >= 0) {
            LOGW("pwr", "eject: hook %u skipped (deadline)", unsigned(i));
            continue;
        }
        const uint32_t w = s_sleepHooks[i]();
        hooks++;
        if (w > 0 && (wakeIn == 0 || w < wakeIn)) {
            wakeIn = w;
        }
    }
    // UI quiesce: it stops LVGL, waits for the last transfer and turns the panel off.
    xSemaphoreTake(s_quiesced, 0);   // drop a stale give
    const uint32_t q0 = millis();
    s_ejecting = true;
    const bool q = xSemaphoreTake(s_quiesced, pdMS_TO_TICKS(200)) == pdTRUE;
    const uint32_t qMs = millis() - q0;

    uint8_t mask = core::kWakeButton;
    if (core::hasCap(board().caps(), core::Cap::ImuWakeInterrupt)) {
        mask |= core::kWakeImu;
    }
    if (wakeIn > 0) {
        mask |= core::kWakeTimer;
    }
    const uint32_t now = millis();
    s_ejectRec = EjectRecord{kEjectMagic, now - t0, gestureMs ? now - gestureMs : 0, recMs, qMs, wakeIn, source};
    LOGW("pwr", "eject: rec %s %lu ms; audio/haptic/net stopped; hooks=%u wake_in=%lu s; ui quiesced%s %lu ms; "
                "sleeping at +%lu ms",
         saved ? "saved" : "NOT saved", (unsigned long)recMs, unsigned(hooks), (unsigned long)wakeIn,
         q ? "" : " (timeout)", (unsigned long)qMs, (unsigned long)(now - t0));
    Serial.flush();
    delay(20);
    if (!board().deepSleep(mask, wakeIn)) {
        LOGE("pwr", "eject unsupported on this board; staying awake");
        s_ejectRec.magic = 0;
        s_ejecting = false;
    }
}

static void cmdFuel(int, const char* const*, core::Reply& r)
{
    PowerSnapshot p = s_snap.read();
    char flow[16];
    if (std::isnan(p.fuelFlowPctPerH)) {
        snprintf(flow, sizeof flow, "--");
    } else {
        snprintf(flow, sizeof flow, "%.1f", p.fuelFlowPctPerH);
    }
    char b[128];
    snprintf(b, sizeof b, "pct=%u chg=%d temp_c=%.1f flow_pph=%s low=%d samples=%u sim=%d valid=%d", p.pmu.battPct,
             p.pmu.charging ? 1 : 0, p.pmu.dieTempC10 / 10.0f, flow, p.lowFuel ? 1 : 0, p.flowSamples, s_simPct,
             p.valid ? 1 : 0);
    r.ok();
    r.line(b);
}

void enableBatteryAlarm() { s_alarmEnabled = true; }

static void cmdFuelSim(int argc, const char* const* argv, core::Reply& r)
{
    if (strcmp(argv[0], "off") == 0) {
        s_simPct = -1;
        s_simChg = -1;
        r.ok();
        return;
    }
    char* end = nullptr;
    long v = strtol(argv[0], &end, 10);
    int chg = -1;
    if (argc >= 2) {
        chg = strcmp(argv[1], "chg") == 0 ? 1 : strcmp(argv[1], "nochg") == 0 ? 0 : -2;
    }
    if (!end || *end != '\0' || v < 0 || v > 100 || chg == -2) {
        r.err("expected 0..100|off [chg|nochg]");
        return;
    }
    s_simPct = int(v);
    s_simChg = chg;
    r.ok();
}

static void cmdEject(int, const char* const*, core::Reply& r)
{
    r.ok();
    r.line("ejecting");
    EventBus::publish(EventType::EjectRequested, 1, millis());
}

static void cmdWake(int, const char* const*, core::Reply& r)
{
    char b[180];
    const core::WakeCause c = board().wakeCause();
    if (c != core::WakeCause::PowerOn && s_ejectRec.magic == kEjectMagic) {
        snprintf(b, sizeof b,
                 "wake=%s eject_ms=%lu gesture_to_sleep_ms=%lu rec_save_ms=%lu quiesce_ms=%lu wake_in=%lu src=%s",
                 core::wakeCauseName(c), (unsigned long)s_ejectRec.requestToSleepMs,
                 (unsigned long)s_ejectRec.gestureToSleepMs, (unsigned long)s_ejectRec.recSaveMs,
                 (unsigned long)s_ejectRec.quiesceMs, (unsigned long)s_ejectRec.wakeInS,
                 s_ejectRec.source == 0 ? "gesture" : "console");
    } else {
        snprintf(b, sizeof b, "wake=%s", core::wakeCauseName(c));
    }
    r.ok();
    r.line(b);
}

static void cmdPwrLog(int, const char* const* argv, core::Reply& r)
{
    if (strcmp(argv[0], "on") == 0) {
        s_profile = true;
    } else if (strcmp(argv[0], "off") == 0) {
        s_profile = false;
    } else {
        r.err("expected on|off");
        return;
    }
    r.ok();
}

void registerCommands()
{
    SerialConsole::add("eject", 0, cmdEject);
    SerialConsole::add("wake", 0, cmdWake);
    SerialConsole::add("pwr log", 1, cmdPwrLog);
    SerialConsole::add("fuel sim", 1, cmdFuelSim);
    SerialConsole::add("fuel", 0, cmdFuel);
}

} // namespace PowerService
