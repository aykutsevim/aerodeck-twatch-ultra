#include "core/EjectGesture.h"

namespace core {

static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }

const char* EjectGesture::cancelName(Cancel c)
{
    switch (c) {
    case Cancel::Timeout: return "timeout";
    case Cancel::Short: return "short";
    case Cancel::Side: return "side";
    case Cancel::NotTap: return "tap";
    case Cancel::Gap: return "gap";
    default: return "none";
    }
}

void EjectGesture::reset()
{
    st_ = State::Idle;
    cancel_ = Cancel::None;
    pressed_ = false;
    dy_ = 0;
}

void EjectGesture::toIdle(Cancel c)
{
    if (st_ != State::Idle) {
        cancel_ = c;
    }
    st_ = State::Idle;
    dy_ = 0;
}

EjectGesture::Cancel EjectGesture::takeCancel()
{
    Cancel c = cancel_;
    cancel_ = Cancel::None;
    return c;
}

float EjectGesture::pullFrac() const
{
    if (st_ != State::Pulling && st_ != State::Fired) {
        return 0.0f;
    }
    float f = float(dy_) / float(p_.travelPx > 0 ? p_.travelPx : 1);
    return f > 1.0f ? 1.0f : f;
}

void EjectGesture::tick(uint32_t ms)
{
    if (st_ == State::Armed && ms - armedSinceMs_ > p_.armedMs) {
        toIdle(Cancel::Timeout);
    } else if (st_ == State::FirstTap && !pressed_ && ms - firstReleaseMs_ > p_.doubleTapGapMs) {
        toIdle(Cancel::Gap);
    }
}

void EjectGesture::press(int32_t x, int32_t y, uint32_t ms)
{
    if (st_ == State::Fired) {
        return;
    }
    tick(ms);   // a press after the armed window or the tap gap starts over
    pressed_ = true;
    pressX_ = x;
    pressY_ = y;
    pressMs_ = ms;
    maxDist_ = 0;
    if (st_ == State::Armed) {
        st_ = State::Pulling;
        dy_ = 0;
    }
}

void EjectGesture::move(int32_t x, int32_t y, uint32_t)
{
    if (!pressed_ || st_ == State::Fired) {
        return;
    }
    const int32_t dx = iabs(x - pressX_);
    const int32_t d = dx > iabs(y - pressY_) ? dx : iabs(y - pressY_);
    if (d > maxDist_) {
        maxDist_ = d;
    }
    if (st_ == State::Pulling) {
        if (dx > p_.sideCancelPx) {
            toIdle(Cancel::Side);
            return;
        }
        dy_ = y - pressY_ > 0 ? y - pressY_ : 0;
        return;
    }
    if (st_ == State::FirstTap && maxDist_ > p_.tapSlopPx) {
        toIdle(Cancel::NotTap);   // a drag is not a tap
    }
}

void EjectGesture::release(int32_t x, int32_t y, uint32_t ms)
{
    if (!pressed_ || st_ == State::Fired) {
        pressed_ = false;
        return;
    }
    move(x, y, ms);
    pressed_ = false;
    if (st_ == State::Pulling) {
        if (float(dy_) >= p_.fireFrac * float(p_.travelPx)) {
            st_ = State::Fired;
        } else {
            toIdle(Cancel::Short);
        }
        return;
    }
    const bool tap = ms - pressMs_ <= p_.tapMaxMs && maxDist_ <= p_.tapSlopPx;
    if (!tap) {
        toIdle(Cancel::NotTap);
        return;
    }
    if (st_ == State::FirstTap && pressMs_ - firstReleaseMs_ <= p_.doubleTapGapMs) {
        st_ = State::Armed;
        armedSinceMs_ = ms;
        return;
    }
    st_ = State::FirstTap;   // first tap (or a new first tap after a slow second one)
    firstReleaseMs_ = ms;
}

} // namespace core
