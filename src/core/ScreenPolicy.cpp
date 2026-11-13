#include "core/ScreenPolicy.h"

namespace core {

uint32_t ScreenPolicy::timeoutMs(uint8_t idx)
{
    static const uint32_t kMs[kTimeoutCount] = {10000, 15000, 30000, 60000, 300000, 0};
    return idx < kTimeoutCount ? kMs[idx] : 15000;
}

const char* ScreenPolicy::causeName(Cause c)
{
    switch (c) {
    case Cause::Boot: return "boot";
    case Cause::Button: return "button";
    case Cause::Raise: return "raise";
    case Cause::Alert: return "alert";
    case Cause::Touch: return "touch";
    default: return "cmd";
    }
}

void ScreenPolicy::configure(uint8_t timeoutIdx, bool raiseEnabled, bool raiseAvailable)
{
    timeoutIdx_ = timeoutIdx < kTimeoutCount ? timeoutIdx : 1;
    raiseEnabled_ = raiseEnabled;
    raiseAvailable_ = raiseAvailable;
}

void ScreenPolicy::begin(uint32_t nowMs)
{
    on_ = true;
    cause_ = Cause::Boot;
    lastWakeMs_ = nowMs;
    pending_ = Action::None;
    suppress_ = false;
}

void ScreenPolicy::requestOn(Cause c, uint32_t nowMs)
{
    cause_ = c;
    lastWakeMs_ = nowMs;
    pending_ = Action::TurnOn;
}

bool ScreenPolicy::onButton(uint32_t nowMs)
{
    if (on_ && pending_ != Action::TurnOff) {
        lastWakeMs_ = nowMs;   // counts as input
        return false;
    }
    requestOn(Cause::Button, nowMs);
    return true;
}

bool ScreenPolicy::onRaise(uint32_t nowMs)
{
    if (on_ || !raiseEnabled_ || !raiseAvailable_) {
        return false;
    }
    requestOn(Cause::Raise, nowMs);
    return true;
}

bool ScreenPolicy::onAlert(AlertLevel lvl, uint32_t nowMs)
{
    if (lvl != AlertLevel::Caution && lvl != AlertLevel::Warning) {
        return false;
    }
    if (on_) {
        lastWakeMs_ = nowMs;   // extend the timeout
        return false;
    }
    requestOn(Cause::Alert, nowMs);
    return true;
}

void ScreenPolicy::command(bool on, uint32_t nowMs)
{
    if (on) {
        requestOn(Cause::Cmd, nowMs);
    } else {
        pending_ = Action::TurnOff;
    }
}

uint32_t ScreenPolicy::idleMs(uint32_t nowMs, uint32_t inactiveMs) const
{
    uint32_t sinceWake = nowMs - lastWakeMs_;
    return inactiveMs < sinceWake ? inactiveMs : sinceWake;
}

ScreenPolicy::Action ScreenPolicy::update(uint32_t nowMs, uint32_t inactiveMs, bool setupShown, bool touchDown)
{
    Action a = Action::None;
    if (pending_ == Action::TurnOn) {
        pending_ = Action::None;
        if (!on_) {
            on_ = true;
            a = Action::TurnOn;
            suppress_ = true;
            sawDown_ = false;
            suppressSinceMs_ = nowMs;
        }
    } else if (pending_ == Action::TurnOff) {
        pending_ = Action::None;
        if (on_) {
            on_ = false;
            return Action::TurnOff;
        }
    }

    if (suppress_) {
        if (touchDown) {
            sawDown_ = true;
        } else if (sawDown_ || nowMs - suppressSinceMs_ >= kSuppressMs) {
            suppress_ = false;
        }
    }

    if (a == Action::None && on_ && !setupShown) {
        uint32_t t = timeoutMs(timeoutIdx_);
        if (t != 0 && idleMs(nowMs, inactiveMs) >= t) {
            on_ = false;
            suppress_ = false;
            return Action::TurnOff;
        }
    }
    return a;
}

} // namespace core
