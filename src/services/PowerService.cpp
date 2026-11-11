#include "services/PowerService.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "board/IBoard.h"
#include "services/EventBus.h"
#include "services/AlertManager.h"
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

void tick(uint32_t nowMs)
{
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

void registerCommands()
{
    SerialConsole::add("fuel sim", 1, cmdFuelSim);
    SerialConsole::add("fuel", 0, cmdFuel);
}

} // namespace PowerService
