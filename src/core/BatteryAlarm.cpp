#include "core/BatteryAlarm.h"

namespace core {

BattAction BatteryAlarm::update(int pct, bool charging, bool valid)
{
    if (!valid) {
        return BattAction::None;
    }
    if (charging) {
        if (s_ != BattState::Normal) {
            s_ = BattState::Normal;
            return BattAction::ClearAll;
        }
        return BattAction::None;
    }
    switch (s_) {
    case BattState::Normal:
        if (pct <= kPullUpAtOrBelow) {
            s_ = BattState::PullUp;
            return BattAction::RaisePullUp;
        }
        if (pct < kStallBelow) {
            s_ = BattState::Stall;
            return BattAction::RaiseStall;
        }
        return BattAction::None;
    case BattState::Stall:
        if (pct <= kPullUpAtOrBelow) {
            s_ = BattState::PullUp;
            return BattAction::RaisePullUp;
        }
        if (pct >= kStallClearAt) {
            s_ = BattState::Normal;
            return BattAction::ClearAll;
        }
        return BattAction::None;
    case BattState::PullUp:
        if (pct >= kStallClearAt) {
            s_ = BattState::Normal;
            return BattAction::ClearAll;
        }
        if (pct >= kPullUpClearAt) {
            s_ = BattState::Stall;
            return BattAction::RaiseStallAcked;
        }
        return BattAction::None;
    }
    return BattAction::None;
}

} // namespace core
