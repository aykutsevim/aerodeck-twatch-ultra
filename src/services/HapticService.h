// AeroDeck haptic service (S4): plays haptic patterns with priority so a click never cuts a warning.
// Thread-safe; click() may be called from the UI task (one ~0.5 ms I2C write). Policy: core::HapticPolicy.
#pragma once
#include <cstdint>
#include "core/HapticPolicy.h"

namespace HapticService {
void begin();                               // registers `ad haptic`
// Plays the pattern with the current vibration level / quiet mode. Returns false when suppressed or unavailable.
bool play(core::Pattern p);
void click();                               // UI click (page change, control)
void stopWarning();                         // stop the stick shaker if it is the pattern playing (acknowledge)
} // namespace HapticService
