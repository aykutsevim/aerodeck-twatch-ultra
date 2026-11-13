// Hot loop on the UI task (every flushed pixel in NVG): optimise for speed even in -Os builds (research R1).
#pragma GCC optimize("O3")
#include "core/NvgColor.h"

namespace core {
namespace NvgColor {

namespace {

// RGB565 = RRRRRGGG GGGBBBBB. Luminance in 6-bit units (white = 63), split by byte:
//   HI[h] = round(r5/31 × 0.299 × 63) + round(gHigh/63 × 0.587 × 63), gHigh = (h & 7) << 3
//   LO[l] = round(gLow/63 × 0.587 × 63) + round(b5/31 × 0.114 × 63),  gLow = l >> 5
// The sum is ≤ 63 (white: 19 + 33 + 4 + 7). OUT/OUTSW map lum6 onto the #00FF66 ramp (native / byte-swapped).
struct Tables {
    uint8_t hi[256];
    uint8_t lo[256];
    uint16_t out[64];
    uint16_t outSw[64];
    Tables()
    {
        auto rnd = [](int num, int den) { return (2 * num + den) / (2 * den); };   // round(num/den), num ≥ 0
        for (int h = 0; h < 256; h++) {
            const int r5 = h >> 3;
            const int gHigh = (h & 7) << 3;
            hi[h] = uint8_t(rnd(r5 * 299 * 63, 31 * 1000) + rnd(gHigh * 587, 1000));
        }
        for (int l = 0; l < 256; l++) {
            const int gLow = l >> 5;
            const int b5 = l & 31;
            lo[l] = uint8_t(rnd(gLow * 587, 1000) + rnd(b5 * 114 * 63, 31 * 1000));
        }
        for (int i = 0; i < 64; i++) {
            const uint16_t v = uint16_t((i << 5) | ((i * 12 + 31) / 63));   // blue = round(i × 12 / 63)
            out[i] = v;
            outSw[i] = uint16_t((v >> 8) | (v << 8));
        }
    }
};

const Tables& tables()
{
    static Tables t;   // RAM (.bss), not flash: read per pixel
    return t;
}

inline unsigned lum(const Tables& t, uint16_t p)
{
    unsigned v = unsigned(t.hi[p >> 8]) + t.lo[p & 0xFF];
    return v > 63 ? 63 : v;
}

} // namespace

uint16_t toGreen565(uint16_t p)
{
    const Tables& t = tables();
    return t.out[lum(t, p)];
}

void apply(uint16_t* px, size_t n)
{
    const Tables& t = tables();
    for (size_t i = 0; i < n; i++) {
        px[i] = t.out[lum(t, px[i])];
    }
}

void applySwapped(uint16_t* px, size_t n)
{
    const Tables& t = tables();
    const uint8_t* hi = t.hi;
    const uint8_t* lo = t.lo;
    const uint16_t* o = t.outSw;
    size_t i = 0;
    // Two pixels per 32-bit word when aligned (LVGL draw buffers are). HI + LO ≤ 63 by construction: no clamp.
    if ((reinterpret_cast<uintptr_t>(px) & 3) == 0) {
        uint32_t* w = reinterpret_cast<uint32_t*>(px);
        const size_t nw = n / 2;
        size_t k = 0;
        for (; k + 2 <= nw; k += 2) {
            const uint32_t v0 = w[k];
            const uint32_t v1 = w[k + 1];
            const unsigned a0 = unsigned(hi[(v0 >> 8) & 0xFF]) + lo[v0 & 0xFF];
            const unsigned b0 = unsigned(hi[v0 >> 24]) + lo[(v0 >> 16) & 0xFF];
            const unsigned a1 = unsigned(hi[(v1 >> 8) & 0xFF]) + lo[v1 & 0xFF];
            const unsigned b1 = unsigned(hi[v1 >> 24]) + lo[(v1 >> 16) & 0xFF];
            w[k] = uint32_t(o[a0]) | (uint32_t(o[b0]) << 16);
            w[k + 1] = uint32_t(o[a1]) | (uint32_t(o[b1]) << 16);
        }
        for (; k < nw; k++) {
            const uint32_t v = w[k];
            const unsigned a = unsigned(hi[(v >> 8) & 0xFF]) + lo[v & 0xFF];
            const unsigned b = unsigned(hi[v >> 24]) + lo[(v >> 16) & 0xFF];
            w[k] = uint32_t(o[a]) | (uint32_t(o[b]) << 16);
        }
        i = nw * 2;
    }
    for (; i < n; i++) {
        px[i] = o[lum(t, px[i])];
    }
}

} // namespace NvgColor
} // namespace core
