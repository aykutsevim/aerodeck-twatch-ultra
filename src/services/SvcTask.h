// AeroDeck service task: core-0 cooperative 50 ms tick (SAS §5.1). Never touches LVGL.
#pragma once
#include <cstdint>

namespace SvcTask {
using TickFn = void (*)(uint32_t nowMs);
constexpr int kMaxTicks = 12;   // S4: 8 in use (alerts added); headroom for S5 audio
constexpr uint32_t kPeriodMs = 50;
bool addTick(TickFn fn);   // call before begin()
void begin();
} // namespace SvcTask
