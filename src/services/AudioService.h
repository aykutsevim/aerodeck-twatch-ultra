// AeroDeck audio (S5): clip queue, WAV streaming from the FAT partition, volume, amplifier lease, hourly chime.
// research R4–R6, data-model §7. The `audio` task (core 0, prio 5) owns the speaker; every API is thread-safe and
// non-blocking (safe from the UI task: it only queues).
#pragma once
#include <cstdint>
#include "core/Alerts.h"
#include "core/SoundQueue.h"

namespace AudioService {

enum class PlayResult : uint8_t { Queued, Suppressed, Duplicate, Full, NoClip };

struct AudioStatus {
    bool ampOn;
    core::Clip playing;   // None when idle
    uint8_t queued;
    uint32_t played;
    uint32_t skipped;
};

void begin();                 // after PowerService::begin and TimeService::begin; starts the task, `ad audio …`
void tick(uint32_t nowMs);    // svc task: hourly chime scheduler (FR-009)
// Queues a clip unless the gates (quiet mode, volume Off, quiet hours) suppress it; `reason` gets the gate name.
PlayResult play(core::Clip c, core::SoundPrio p, const char** reason = nullptr);
void stop(core::SoundPrio minPrio, const char* why);   // stop the playing clip if prio >= minPrio; purge the queue
void stopAlerts();                                      // acknowledge: stop caution/warning callouts (FR-007)
AudioStatus status();

} // namespace AudioService
