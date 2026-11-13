// AeroDeck core: NVG recolouring (S7, FR-013, research R1, data-model §5). Pure; host-tested.
// Maps any RGB565 pixel by its luminance onto the black → #00FF66 ramp (white → 0x07EC). Red is always 0.
// Fast path: luminance = HI[high byte] + LO[low byte] (two 256-entry tables), output from a 64-entry table.
#pragma once
#include <cstddef>
#include <cstdint>

namespace core {
namespace NvgColor {

uint16_t toGreen565(uint16_t rgb565);
// In place over n pixels (native RGB565 in, native RGB565 out).
void apply(uint16_t* px, size_t n);
// In place over n pixels, output byte-swapped (for panels that take swapped RGB565; replaces the separate
// swap pass of the flush). Input is native RGB565.
void applySwapped(uint16_t* px, size_t n);

} // namespace NvgColor
} // namespace core
