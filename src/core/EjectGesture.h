// AeroDeck core: Pull-to-Eject gesture (S7, FR-007, NFR-USE-003, research R8, data-model §4). Pure; host-tested.
// Double tap (gap ≤ 400 ms) → Armed (≤ 3 s) → press and pull down ≥ 40 % of the travel → Fired.
// Anything else (timeout, short pull, sideways move, long press, slow taps) disarms without effect.
#pragma once
#include <cstdint>

namespace core {

class EjectGesture {
public:
    enum class State : uint8_t { Idle, FirstTap, Armed, Pulling, Fired };
    enum class Cancel : uint8_t { None, Timeout, Short, Side, NotTap, Gap };

    struct Params {
        uint32_t tapMaxMs = 300;
        int32_t tapSlopPx = 10;
        uint32_t doubleTapGapMs = 400;
        uint32_t armedMs = 3000;
        int32_t travelPx = 171;   // S(100) on the Ultra
        float fireFrac = 0.40f;
        int32_t sideCancelPx = 20;
    };

    EjectGesture() = default;
    explicit EjectGesture(const Params& p) : p_(p) {}

    void press(int32_t x, int32_t y, uint32_t ms);
    void move(int32_t x, int32_t y, uint32_t ms);
    void release(int32_t x, int32_t y, uint32_t ms);
    void tick(uint32_t ms);
    void reset();

    State state() const { return st_; }
    float pullFrac() const;
    bool fired() const { return st_ == State::Fired; }
    // The reason of the last return to Idle (for the `[eject] disarmed (…)` log); cleared by takeCancel().
    Cancel takeCancel();
    static const char* cancelName(Cancel c);
    const Params& params() const { return p_; }

private:
    void toIdle(Cancel c);

    Params p_;
    State st_ = State::Idle;
    Cancel cancel_ = Cancel::None;
    bool pressed_ = false;
    int32_t pressX_ = 0, pressY_ = 0;
    uint32_t pressMs_ = 0;
    int32_t maxDist_ = 0;
    uint32_t firstReleaseMs_ = 0;
    uint32_t armedSinceMs_ = 0;
    int32_t dy_ = 0;
};

} // namespace core
