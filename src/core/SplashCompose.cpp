#include "core/SplashCompose.h"

namespace core {

const char* splashSkipName(SplashSkip s)
{
    switch (s) {
    case SplashSkip::Wake: return "wake";
    case SplashSkip::Safe: return "safe";
    case SplashSkip::NoArt: return "none";
    case SplashSkip::NoMem: return "nomem";
    default: return "-";
    }
}

namespace SplashCompose {

void row(const uint8_t* a8, uint16_t aw, uint16_t ah, uint16_t W, uint16_t H, uint16_t y, uint16_t fg565, bool swap,
         uint16_t* out)
{
    for (uint16_t x = 0; x < W; x++) {
        out[x] = 0;
    }
    const int x0 = (int(W) - int(aw)) / 2, y0 = (int(H) - int(ah)) / 2;
    const int ay = int(y) - y0;
    if (!a8 || ay < 0 || ay >= int(ah)) {
        return;
    }
    const uint32_t fr = fg565 >> 11, fgc = (fg565 >> 5) & 0x3F, fb = fg565 & 0x1F;
    const uint8_t* src = a8 + size_t(ay) * aw;
    for (int ax = 0; ax < int(aw); ax++) {
        const int x = x0 + ax;
        if (x < 0 || x >= int(W)) {
            continue;
        }
        const uint32_t a = src[ax];
        if (a == 0) {
            continue;
        }
        const uint16_t r = uint16_t((fr * a + 127) / 255), g = uint16_t((fgc * a + 127) / 255),
                       b = uint16_t((fb * a + 127) / 255);
        const uint16_t p = uint16_t((r << 11) | (g << 5) | b);
        out[x] = swap ? uint16_t((p >> 8) | (p << 8)) : p;
    }
}

static bool insideRounded(int x, int y, int W, int H, int r)
{
    if (x < 0 || y < 0 || x > W || y > H) {
        return false;
    }
    if (r <= 0) {
        return true;
    }
    const int cx = x < r ? r : (x > W - r ? W - r : x);
    const int cy = y < r ? r : (y > H - r ? H - r : y);
    return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r;
}

bool fitsRounded(const uint8_t* a8, uint16_t aw, uint16_t ah, uint16_t W, uint16_t H, uint16_t r, uint16_t margin)
{
    const int x0 = (int(W) - int(aw)) / 2, y0 = (int(H) - int(ah)) / 2;
    const int iw = int(W) - 2 * margin, ih = int(H) - 2 * margin, ir = int(r) > margin ? int(r) - margin : 0;
    for (int y = 0; y < int(ah); y++) {
        for (int x = 0; x < int(aw); x++) {
            if (!a8[size_t(y) * aw + x]) {
                continue;
            }
            // the pixel covers [px, px+1] x [py, py+1]: all four corners must be inside
            const int px = x0 + x - margin, py = y0 + y - margin;
            if (!insideRounded(px, py, iw, ih, ir) || !insideRounded(px + 1, py, iw, ih, ir) ||
                !insideRounded(px, py + 1, iw, ih, ir) || !insideRounded(px + 1, py + 1, iw, ih, ir)) {
                return false;
            }
        }
    }
    return true;
}

bool shouldShow(BootMode mode, WakeCause wake, bool hasArt, SplashSkip& why)
{
    if (!hasArt) {
        why = SplashSkip::NoArt;
    } else if (mode != BootMode::Normal) {
        why = SplashSkip::Safe;
    } else if (wake != WakeCause::PowerOn) {
        why = SplashSkip::Wake;
    } else {
        why = SplashSkip::None;
    }
    return why == SplashSkip::None;
}

} // namespace SplashCompose
} // namespace core
