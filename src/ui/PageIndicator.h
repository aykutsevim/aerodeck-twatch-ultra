// AeroDeck page indicator: dots + title on the top layer, shown ~1 s after each page change (FR-005).
#pragma once

namespace PageIndicator {
void begin();
void show(int index, int count, const char* title);
} // namespace PageIndicator
