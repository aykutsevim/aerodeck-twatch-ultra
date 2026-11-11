#include "core/Attitude.h"
#include <cmath>

namespace core {

static constexpr float kR2D = 57.29577951f;

static float wrap180(float d)
{
    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

AttitudeOut Attitude::update(float gx, float gy, float gz)
{
    const float g[3] = {gx, gy, gz};
    const float ax = map_.sx * g[map_.ix];
    const float ay = map_.sy * g[map_.iy];
    const float az = map_.sz * g[map_.iz];

    const float rawPitch = std::atan2(-ay, std::sqrt(ax * ax + az * az)) * kR2D;
    const bool vertical = std::fabs(rawPitch) > kVerticalDeg;
    float rawRoll = vertical ? roll_ : std::atan2(ax, az) * kR2D;

    if (!primed_) {
        pitch_ = rawPitch;
        roll_ = vertical ? 0.0f : rawRoll;
        primed_ = true;
    } else {
        pitch_ += kAlpha * (rawPitch - pitch_);
        roll_ = wrap180(roll_ + kAlpha * wrap180(rawRoll - roll_));   // wrap-aware EMA
    }
    return AttitudeOut{pitch_, roll_};
}

} // namespace core
