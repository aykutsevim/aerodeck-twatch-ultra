// Amber caution banner on the top layer; cycles through active cautions every 3 s (FR-015).
#pragma once

namespace CautionBanner {
void begin();
void refresh();   // call on CautionChanged (UI task)
} // namespace CautionBanner
