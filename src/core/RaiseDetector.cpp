#include "core/RaiseDetector.h"
#include <cmath>

namespace core {

static constexpr float kR2D = 57.29577951f;

void RaiseDetector::reset()
{
    st_ = St::Idle;
    awaySeen_ = false;
    armed_ = false;
}

bool RaiseDetector::feed(float x, float y, float z, uint32_t ms)
{
    const float n = std::sqrt(x * x + y * y + z * z);
    if (!(n > 1e-6f)) {
        return false;
    }
    x /= n;
    y /= n;
    z /= n;
    const float pitch = std::atan2(-y, std::sqrt(x * x + z * z)) * kR2D;
    const float roll = std::atan2(x, z) * kR2D;
    const bool inView = z >= kViewMinZ && std::fabs(pitch) <= kViewMaxPitchDeg && std::fabs(roll) <= kViewMaxRollDeg;
    const bool away = z <= kAwayMaxZ || std::fabs(pitch) >= kAwayPitchDeg || std::fabs(roll) >= kAwayRollDeg;

    if (away) {
        if (!awaySeen_) {
            awaySeen_ = true;
            awayStartMs_ = ms;
        }
        if (ms - awayStartMs_ >= kAwayMinMs) {
            armed_ = true;
        }
        lastAwayMs_ = ms;
        st_ = St::Away;
        return false;
    }
    awaySeen_ = false;   // an away period ends at the first non-away sample

    if (!inView) {
        // In between (rising, or drifting): a hold in progress is broken.
        if (st_ == St::Holding) {
            st_ = St::Idle;
        }
        if (armed_ && ms - lastAwayMs_ > kRiseWindowMs) {
            armed_ = false;   // too slow
        }
        return false;
    }

    // In the viewing pose.
    if (st_ != St::Holding) {
        if (!armed_ || ms - lastAwayMs_ > kRiseWindowMs) {
            armed_ = false;
            st_ = St::Idle;
            return false;
        }
        st_ = St::Holding;
        holdStartMs_ = ms;
    }
    if (ms - holdStartMs_ >= kHoldMs) {
        st_ = St::Idle;
        armed_ = false;   // one fire per raise; a new away period is needed
        return true;
    }
    return false;
}

} // namespace core
