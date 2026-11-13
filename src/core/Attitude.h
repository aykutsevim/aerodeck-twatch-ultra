// AeroDeck core: pitch/roll from a gravity vector (data-model §1, research R1). Host-tested.
#pragma once
#include <cstdint>

namespace core {

// Maps sensor axes to the watch frame: a[k] = s_k * g[i_k]. Verified on the device (spike T014).
struct AxisMap {
    uint8_t ix, iy, iz;
    int8_t sx, sy, sz;
};
constexpr AxisMap kIdentityAxes{0, 1, 2, 1, 1, 1};
// S7 (wearer request): the horizon is level at a chosen viewing tilt (setting `attref`, 0 = flat as in S1,
// 90 = upright). Measured hub frame: up = +z flat, -y upright (12 o'clock up), +x when 12 o'clock points right.
// The up vector is rotated about the watch's 3-9 o'clock axis by the tilt before the flat formulas, so the watch
// held with its face tilted `tilt` degrees from the ground (top edge raised) reads level.

struct AttitudeOut {
    float pitchDeg;   // + nose up, -90..90
    float rollDeg;    // + right wing down, -180..180
};

class Attitude {
public:
    static constexpr float kAlpha = 0.5f;          // EMA weight of the new sample
    static constexpr float kVerticalDeg = 80.0f;   // above this |pitch|, roll is held

    AttitudeOut update(float gx, float gy, float gz);
    void reset() { primed_ = false; }
    void setAxisMap(const AxisMap& m) { map_ = m; }
    const AxisMap& axisMap() const { return map_; }
    void setReferenceTiltDeg(float deg);   // 0 = level when flat (S1), 90 = level when upright
    float referenceTiltDeg() const { return tiltDeg_; }

private:
    AxisMap map_ = kIdentityAxes;
    float tiltDeg_ = 0.0f;
    float tiltSin_ = 0.0f;
    float tiltCos_ = 1.0f;
    bool primed_ = false;
    float pitch_ = 0.0f;
    float roll_ = 0.0f;
};

} // namespace core
