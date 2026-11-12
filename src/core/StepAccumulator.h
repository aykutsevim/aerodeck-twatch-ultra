// AeroDeck core: the sensor hub's cumulative step count → step deltas (S6 data-model §3, FR-010). Pure, host-tested.
#pragma once
#include <cstdint>

namespace core {

class StepAccumulator {
public:
    void reset(uint32_t nowMs);   // the hub (re)started: its count is 0
    // Steps to add. A count below the last one means the hub restarted (delta = raw). A delta above
    // 6 steps/s × elapsed + 20 is implausible: rejected (0) and the baseline moves to raw.
    uint32_t feed(uint32_t raw, uint32_t nowMs, bool& rejected);
    uint32_t last() const { return last_; }

private:
    uint32_t last_ = 0;
    uint32_t lastMs_ = 0;
};

} // namespace core
