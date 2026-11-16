// AeroDeck TCAS LoRa radar (S9, FR-001…011, research R1–R8b). Runs on the svc task; a dedicated `radio` task
// (core 0) owns the SX1262 while TCAS is on: DIO1-driven RX and blocking transmits never stall the svc tick.
// RailLease(Lora) is held only while TCAS is on (FR-004). Every beacon passes the region limits and the rolling
// 1 %/h duty budget (core::DutyCycle, kept in RTC memory across eject and soft reset) before it is queued.
#pragma once
#include <cstdint>
#include "core/TcasTable.h"
#include "services/Snapshot.h"

struct TcasSnapshot {
    bool on;          // the `tcas` setting
    bool running;     // the radio task is up and the radio started
    bool radioOk;     // false → RADIO FAIL (Ultra) / RADIO N/A (no radio on this board)
    bool radioAvail;  // the board has a LoRa radio at all
    uint8_t region;
    char regionName[8];
    float freqMHz;
    float bwKHz;
    int8_t powerDbm;
    uint8_t sf;
    char callsign[9];
    uint32_t nodeId;
    uint16_t intervalS;
    uint32_t sent, delayed, skipped, txFail;
    uint32_t rx, foreign, crc;
    uint32_t dutyUsedMs, dutyBudgetMs;
    uint32_t lastAirMs;
    uint8_t n;
    core::Contact contacts[core::TcasTable::kMax];
    uint32_t atMs;    // millis() of this snapshot (contact ages = atMs - lastSeenMs)
    uint32_t seq;
};

namespace TcasService {
void begin();                    // after Settings/Power/Alerts; registers commands and the pre-sleep hook
void tick(uint32_t nowMs);       // svc task
void onProbeDone();              // deferred peripheral probe finished: the radio may be started
void setEnabled(bool on);        // any task (writes the `tcas` setting)
const Snapshot<TcasSnapshot>& status();
} // namespace TcasService
