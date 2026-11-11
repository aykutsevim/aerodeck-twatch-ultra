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

private:
    AxisMap map_ = kIdentityAxes;
    bool primed_ = false;
    float pitch_ = 0.0f;
    float roll_ = 0.0f;
};

} // namespace core
