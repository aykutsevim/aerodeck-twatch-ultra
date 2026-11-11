// AeroDeck core: low-battery alarm state machine (S4 data-model §3, spec FR-012). Pure, host-tested.
// STALL below 15 % (clears at >= 17 %), PULL UP at <= 5 % (steps back to an acknowledged STALL at >= 7 %);
// charging clears everything; invalid readings are ignored.
#pragma once
#include <cstdint>

namespace core {

enum class BattState : uint8_t { Normal, Stall, PullUp };
enum class BattAction : uint8_t { None, RaiseStall, RaiseStallAcked, RaisePullUp, ClearAll };

class BatteryAlarm {
public:
    static constexpr int kStallBelow = 15;
    static constexpr int kStallClearAt = 17;
    static constexpr int kPullUpAtOrBelow = 5;
    static constexpr int kPullUpClearAt = 7;
    BattAction update(int pct, bool charging, bool valid);
    BattState state() const { return s_; }

private:
    BattState s_ = BattState::Normal;
};

} // namespace core
