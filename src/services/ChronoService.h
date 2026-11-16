// AeroDeck drop-tank chronograph (S8, FR-001…013, research R1–R7). Runs on the svc task; commands are queued
// from the UI and the console. State persists in NVS namespace `chrono` (on changes only); the eject pre-sleep
// hook asks to wake 5 s before the next event (or postpones the eject in the last 6 s).
#pragma once
#include <cstdint>
#include "core/ChronoModel.h"
#include "services/Snapshot.h"

struct ChronoSnapshot {
    core::ChronoModel::State state;
    core::ChronoModel::Mode mode;
    core::ChronoModel::Phase phase;
    int32_t remainingMs;
    int32_t intervalMs;
    float fraction;
    uint16_t cycles;
    bool late;
    int64_t endUtcMs;
    uint32_t saves;   // NVS writes since boot
    uint32_t seq;
};

namespace ChronoService {
enum class Cmd : uint8_t { Start, Pause, Resume, Reset, Toggle, StartTest, PomoTest, Reconfigure };
void begin();                      // after AlertManager/TimeService/PowerService; loads and restores; commands; hook
void tick(uint32_t nowMs);         // svc task (every 50 ms tick)
void post(Cmd c, uint32_t a = 0, uint32_t b = 0);   // any task
const Snapshot<ChronoSnapshot>& status();
} // namespace ChronoService
