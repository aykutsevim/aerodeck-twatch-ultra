// AeroDeck core: boot splash composition (feature 011, data-model.md). Pure, host-tested.
// The board pushes the splash straight to the panel right after LilyGoLib's panel init, strip by strip; this module
// renders those rows (an A8 mask centred on black, tinted with one colour) and decides whether to show it at all.
#pragma once
#include <cstddef>
#include <cstdint>
#include "core/BootGuard.h"
#include "core/Types.h"

namespace core {

enum class SplashSkip : uint8_t { None, Wake, Safe, NoArt, NoMem };
const char* splashSkipName(SplashSkip s);   // "-", "wake", "safe", "none", "nomem"

namespace SplashCompose {

// Screen row y (of W×H) with the aw×ah mask centred (x0 = (W−aw)/2, y0 = (H−ah)/2); outside the mask black,
// inside fg × alpha / 255 per channel (RGB565). swap: byte-swapped output (the panel's order).
void row(const uint8_t* a8, uint16_t aw, uint16_t ah, uint16_t W, uint16_t H, uint16_t y, uint16_t fg565, bool swap,
         uint16_t* out);

// True if every alpha>0 pixel of the centred mask lies inside the W×H screen with rounded corners (radius r),
// shrunk by margin px on each side.
bool fitsRounded(const uint8_t* a8, uint16_t aw, uint16_t ah, uint16_t W, uint16_t H, uint16_t r, uint16_t margin);

// The show rule (research R3): a full start (PowerOn), boot mode Normal, art linked in.
bool shouldShow(BootMode mode, WakeCause wake, bool hasArt, SplashSkip& why);

} // namespace SplashCompose
} // namespace core
