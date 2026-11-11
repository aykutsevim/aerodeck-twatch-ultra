#include "core/Fuel.h"
#include <cmath>
#include <cstdlib>

namespace core {

void FuelFlow::addSample(uint32_t minute, uint8_t pct, bool charging)
{
    if (charging) {
        reset();
        return;
    }
    if (n_ > 0) {
        const Sample& last = at(n_ - 1);
        if (minute == last.minute) {
            return;   // one sample per minute
        }
        if (std::abs(int(pct) - int(last.pct)) > kJumpPct) {
            reset();   // gauge recalibration / battery swap: start over
        }
    }
    ring_[head_] = Sample{minute, pct};
    head_ = (head_ + 1) % kCapacity;
    if (n_ < kCapacity) {
        n_++;
    }
}

float FuelFlow::ratePctPerH() const
{
    if (n_ < 2) {
        return NAN;
    }
    const uint32_t t0 = at(0).minute;
    if (at(n_ - 1).minute - t0 < kMinSpanMin) {
        return NAN;
    }
    // Least-squares slope of pct over minutes.
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (size_t i = 0; i < n_; i++) {
        const double x = double(at(i).minute - t0);
        const double y = double(at(i).pct);
        sx += x;
        sy += y;
        sxx += x * x;
        sxy += x * y;
    }
    const double n = double(n_);
    const double den = n * sxx - sx * sx;
    if (den <= 0.0) {
        return NAN;
    }
    const double slope = (n * sxy - sx * sy) / den;   // %/min (negative while draining)
    return float(-slope * 60.0);
}

bool FuelState::update(uint8_t pct, bool charging)
{
    if (charging || pct > kClearAbove) {
        low_ = false;
    } else if (pct < kLowBelow) {
        low_ = true;
    }
    return low_;
}

} // namespace core
