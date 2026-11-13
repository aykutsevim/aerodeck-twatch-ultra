// AeroDeck attitude: BHI260AP gravity → pitch/roll/LOCK snapshot (research R1, ADR-010).
// The gravity callback runs on the UI task (inside IBoard::loop); the snapshot is read by the PFD.
#pragma once
#include <cstdint>
#include "services/Snapshot.h"

enum class AttStatus : uint8_t { Align, Ok, Fail };

struct AttitudeSnapshot {
    float pitchDeg;
    float rollDeg;
    bool level;
    AttStatus status;
    uint32_t sampleMs;
    uint32_t seq;
};

namespace AttitudeService {
constexpr float kRateHz = 100.0f;
void begin();                       // status Align; registers `ad att`
void onImuReady(bool imuOk);        // from the deferred-probe task (core 0)
const Snapshot<AttitudeSnapshot>& att();
void tick(uint32_t nowMs);          // svc task: sample watchdog, rate statistics
void notifyRendered(uint32_t sampleMs);   // UI task: latency of the sample just drawn (R9)
// S7: every gravity sample is also passed to this tap (UI task, O(1); raise-to-wake). Set by the UI layer.
using GravityTap = void (*)(float x, float y, float z, uint32_t ms);
void setGravityTap(GravityTap tap);
// S7: horizon level at this face tilt from the ground (setting `attref`, 0 = flat as in S1, 90 = upright).
void setReferenceTilt(uint8_t deg);
} // namespace AttitudeService
