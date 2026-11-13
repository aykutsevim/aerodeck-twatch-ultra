// AeroDeck core: wrist-raise detection from gravity samples (S7, FR-002, research R5, data-model §3). Pure.
// The gravity vector is normalised (the hub's scale is not 1 g), then pitch/roll follow core::Attitude:
// pitch = atan2(-y, sqrt(x² + z²)), roll = atan2(x, z); face up = +z.
// A raise = out of the viewing pose for ≥ kAwayMinMs, then into it within kRiseWindowMs, held for kHoldMs.
#pragma once
#include <cstdint>

namespace core {

class RaiseDetector {
public:
    // Viewing pose (face up, tilted toward the eyes). Tuned on the device (T020, research R5).
    static constexpr float kViewMaxPitchDeg = 45.0f;
    static constexpr float kViewMaxRollDeg = 35.0f;
    static constexpr float kViewMinZ = 0.5f;          // normalised z (face up)
    // "Away" (clearly not looking): hysteresis beyond the viewing limits.
    static constexpr float kAwayPitchDeg = 55.0f;
    static constexpr float kAwayRollDeg = 50.0f;
    static constexpr float kAwayMaxZ = 0.3f;
    static constexpr uint32_t kAwayMinMs = 300;
    static constexpr uint32_t kRiseWindowMs = 1000;
    static constexpr uint32_t kHoldMs = 150;

    // Returns true once per detected raise.
    bool feed(float x, float y, float z, uint32_t ms);
    void reset();

private:
    enum class St : uint8_t { Idle, Away, Holding };
    St st_ = St::Idle;
    bool awaySeen_ = false;       // an away period is in progress
    uint32_t awayStartMs_ = 0;    // start of the current away period
    uint32_t lastAwayMs_ = 0;     // last sample that was away (while armed)
    bool armed_ = false;          // away long enough
    uint32_t holdStartMs_ = 0;
};

} // namespace core
