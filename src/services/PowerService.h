// AeroDeck power: reference-counted rail leases (NFR-PWR-001) and 1 Hz PMU snapshot.
#pragma once
#include "core/Types.h"
#include "services/Snapshot.h"

struct PowerSnapshot {
    core::PmuReading pmu;
    bool valid;
    uint32_t atMs;
    float fuelFlowPctPerH;   // S1: estimated drain rate, NaN = unavailable (core::FuelFlow)
    bool lowFuel;            // S1: LOW FUEL hysteresis (core::FuelState)
    uint8_t flowSamples;
    uint32_t seq;
};

class RailLease {
public:
    explicit RailLease(core::Rail r);
    ~RailLease();
    RailLease(RailLease&& o) noexcept;
    RailLease& operator=(RailLease&& o) noexcept;
    RailLease(const RailLease&) = delete;
    RailLease& operator=(const RailLease&) = delete;

private:
    core::Rail rail_;
    bool held_;
};

namespace PowerService {
void begin();   // forces optional rails off; registers the svc tick
const Snapshot<PowerSnapshot>& power();
uint8_t refcount(core::Rail r);
// S2: a rail that is not a PMU output (Wi-Fi) is switched by its owner's hook instead of IBoard::setRail.
using RailHook = void (*)(bool on);
void setRailHook(core::Rail r, RailHook fn);
// Re-applies OFF to every optional rail with no lease (e.g. after vendor init powered them).
void enforceIdleRails();
// S4: start the STALL / PULL UP alarm (called when the watch face first appears, spec edge case).
void enableBatteryAlarm();
void tick(uint32_t nowMs);
void registerCommands();   // `ad fuel`, `ad fuel sim`; S7: `ad eject`, `ad wake`, `ad pwr log on|off`

// S7 eject (FR-008/009, research R6). EjectRequested (UI gesture or console) → eject() on the svc task.
// Pre-sleep hooks run in the eject sequence; each returns the wanted wake-up in seconds (0 = none). ≤ 4 hooks.
using PreSleepHook = uint32_t (*)();
bool addPreSleepHook(PreSleepHook fn);
// Quiesce handshake with the UI task: while ejecting() is true the UI stops LVGL, turns the display off and calls
// uiQuiesced() once. If deep sleep turns out to be unsupported, ejecting() goes false again and the UI resumes.
bool ejecting();
void uiQuiesced();
} // namespace PowerService
