// AeroDeck flight recorder (S6): steps → Flight Miles, today's Max G, local-midnight days, 7-day history,
// persistence in NVS namespace `rec` (research R1–R6, data-model §7–8). Runs on the svc task.
#pragma once
#include <cstdint>
#include "core/DayLog.h"
#include "services/Snapshot.h"

struct RecorderSnapshot {
    bool stepsOk;          // hub step counter running
    bool accelOk;          // accelerometer running
    float gNow;            // latest |a| in g
    uint32_t steps;        // today
    uint16_t maxGc;        // today's Max G (centi-g), 0 = none
    int16_t maxGMin;       // local minute of Max G, -1 = none
    bool maxGSat;          // Max G at the sensor's range limit
    uint8_t strideCm;
    float rangeG;
    core::DayRecord chart[7];   // [0] today, [1..6] the previous days by date
    int32_t today;              // local day number or core::kUndated
    uint32_t lastSaveMs;
    uint32_t saves;
    uint32_t rejectedJumps;
    uint32_t seq;
};

namespace RecorderService {
void begin();                       // loads the saved days, registers `ad rec …` and the shutdown save
void onImuReady(bool imuOk);        // from the deferred probe: start the step counter and accelerometer
void tick(uint32_t nowMs);          // svc task (works at 1 Hz)
const Snapshot<RecorderSnapshot>& status();
bool saveNow(const char* reason);   // svc task; also for S7 (eject)
} // namespace RecorderService
