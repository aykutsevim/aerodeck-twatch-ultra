// UI helper (S4, FR-010): give an LVGL control a crisp click when it is activated. UI task only.
#pragma once
#include <lvgl.h>

namespace HapticClick {
// Buttons/pickers: on CLICKED. Switches/dropdowns: on VALUE_CHANGED (one click per change).
void attach(lv_obj_t* obj, bool onValueChanged = false);
} // namespace HapticClick
