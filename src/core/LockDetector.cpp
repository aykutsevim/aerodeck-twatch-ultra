#include "core/LockDetector.h"
#include <cmath>

namespace core {

bool LockDetector::update(float pitch, float roll, bool statusOk, uint32_t now)
{
    if (!statusOk) {
        locked_ = false;
        timing_ = false;
        return false;
    }
    const float p = std::fabs(pitch);
    const float r = std::fabs(roll);
    if (locked_) {
        if (p > kOutDeg || r > kOutDeg) {
            locked_ = false;
            timing_ = false;
        }
        return locked_;
    }
    if (p <= kInDeg && r <= kInDeg) {
        if (!timing_) {
            timing_ = true;
            since_ = now;
        }
        if (now - since_ >= kHoldMs) {
            locked_ = true;
        }
    } else {
        timing_ = false;
    }
    return locked_;
}

} // namespace core
