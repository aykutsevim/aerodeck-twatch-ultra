#include "core/StepAccumulator.h"

namespace core {

void StepAccumulator::reset(uint32_t nowMs)
{
    last_ = 0;
    lastMs_ = nowMs;
}

uint32_t StepAccumulator::feed(uint32_t raw, uint32_t nowMs, bool& rejected)
{
    rejected = false;
    const uint32_t delta = raw >= last_ ? raw - last_ : raw;
    const uint64_t allowed = uint64_t(6) * uint32_t(nowMs - lastMs_) / 1000u + 20u;
    last_ = raw;
    lastMs_ = nowMs;
    if (delta > allowed) {
        rejected = true;
        return 0;
    }
    return delta;
}

} // namespace core
