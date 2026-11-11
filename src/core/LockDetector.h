// AeroDeck core: level LOCK with hysteresis (FR-003, data-model §2). Host-tested.
#pragma once
#include <cstdint>

namespace core {

class LockDetector {
public:
    static constexpr float kInDeg = 3.0f;
    static constexpr float kOutDeg = 5.0f;
    static constexpr uint32_t kHoldMs = 500;

    bool update(float pitchDeg, float rollDeg, bool statusOk, uint32_t nowMs);
    bool locked() const { return locked_; }

private:
    bool locked_ = false;
    bool timing_ = false;
    uint32_t since_ = 0;
};

} // namespace core
