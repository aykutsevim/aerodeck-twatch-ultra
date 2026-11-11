#include "core/PfdLayout.h"

#include <cmath>

namespace core {

// Rounded-corner geometry: corner circles centred r px in from each edge.
static bool pointInside(float x, float y, int32_t w, int32_t h, int32_t r)
{
    if (x < 0 || y < 0 || x > w || y > h) {
        return false;
    }
    if (r <= 0) {
        return true;
    }
    const float cx = x < r ? r : (x > w - r ? w - r : x);
    const float cy = y < r ? r : (y > h - r ? h - r : y);
    const float dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= float(r) * float(r) + 0.5f;
}

bool insideRounded(const Box& b, int32_t w, int32_t h, int32_t r)
{
    return pointInside(float(b.x), float(b.y), w, h, r) && pointInside(float(b.x + b.w), float(b.y), w, h, r) &&
           pointInside(float(b.x), float(b.y + b.h), w, h, r) &&
           pointInside(float(b.x + b.w), float(b.y + b.h), w, h, r);
}

int32_t minTopForRounded(int32_t x, int32_t bw, int32_t w, int32_t r)
{
    if (r <= 0) {
        return 0;
    }
    // The tighter of the two top corners of the box.
    const int32_t inset = x < (w - (x + bw)) ? x : (w - (x + bw));
    if (inset >= r) {
        return 0;
    }
    const float d = float(r - inset);
    return int32_t(std::ceil(r - std::sqrt(float(r) * r - d * d)));
}

static int32_t sc(float s, int32_t px) { return int32_t(px * s + 0.5f); }

// Size cap (ADR-011, research R2 option a): the horizon is redrawn every frame and the display pipeline tops out
// near 34 FPS for a 280 px instrument; ~238 px plus a stepped heading tape keeps ≥30 FPS on the Ultra.
static int32_t capAdi(int32_t a, float s)
{
    const int32_t cap = sc(s, 140);
    if (a > cap) {
        a = cap;
    }
    return a & ~1;   // even size (the AMOLED rounder works on even coordinates)
}

// Portrait (T-Watch Ultra: 410×502). Top to bottom: heading tape (with MAX G / NM beside it),
// hour tape | ADI | minute tape, EICAS strip, ZULU | LOCAL row. Clears the top caution banner.
static PfdLayout portrait(int32_t w, int32_t h, float s, int32_t cornerR)
{
    const int32_t m = sc(s, 4);
    const int32_t headH = sc(s, 24);
    const int32_t eicasH = sc(s, 36);
    const int32_t tapeW = sc(s, 40);
    const int32_t timeH = sc(s, 34);     // caption + 28 px HUD digits
    const int32_t bannerH = sc(s, 28);   // S0 caution banner height

    int32_t a = capAdi(w - 2 * (tapeW + 2 * m), s);
    const int32_t contentW = a + 2 * (tapeW + m);
    const int32_t x0 = (w - contentW) / 2;
    const int32_t totalH = headH + m + a + m + eicasH + m + timeH;
    int32_t y0 = (h - totalH) / 2;
    if (y0 < bannerH) {
        y0 = bannerH;
    }
    // Rounded screen corners: the outer top boxes (MAX G / NM) must clear the corner arcs.
    const int32_t topMin = minTopForRounded(x0, tapeW, w, cornerR);
    if (y0 < topMin) {
        y0 = topMin;
    }
    if (y0 + totalH > h - m) {   // very short screens: give up the banner gap rather than overflow
        y0 = h - m - totalH;
    }
    if (y0 < 0) {
        y0 = 0;
    }

    PfdLayout L{};
    const int32_t adiX = x0 + tapeW + m;
    L.heading = {adiX, y0, a, headH};
    L.maxg = {x0, y0, tapeW, headH};
    L.nm = {x0 + contentW - tapeW, y0, tapeW, headH};
    L.adi = {adiX, y0 + headH + m, a, a};
    L.hour = {x0, L.adi.y, tapeW, a};
    L.minute = {x0 + contentW - tapeW, L.adi.y, tapeW, a};
    L.eicas = {x0, L.adi.y + a + m, contentW, eicasH};
    const int32_t rowY = L.eicas.y + eicasH + m;
    const int32_t halfW = (contentW - m) / 2;
    L.zulu = {x0, rowY, halfW, timeH};
    L.local = {x0 + contentW - halfW, rowY, halfW, timeH};
    return L;
}

// Landscape / square (e.g. 502×410, T-Watch S3 240×240): tapes beside the ADI, corners below the tapes.
static PfdLayout landscape(int32_t w, int32_t h, float s)
{
    const int32_t m = sc(s, 4);
    const int32_t headH = sc(s, 24);
    const int32_t eicasH = sc(s, 36);
    const int32_t tapeW = sc(s, 52);

    const int32_t byH = h - (m + headH + m) - (m + eicasH + m);
    const int32_t byW = w - 2 * (tapeW + 2 * m);
    const int32_t a = capAdi(byH < byW ? byH : byW, s);

    PfdLayout L{};
    L.adi = {(w - a) / 2, m + headH + m, a, a};
    L.heading = {L.adi.x, m, a, headH};
    L.eicas = {L.adi.x, L.adi.y + a + m, a, eicasH};

    const int32_t sideW = L.adi.x - 2 * m;   // column width left/right of the ADI
    L.maxg = {m, m, sideW, headH};
    L.nm = {w - m - sideW, m, sideW, headH};
    L.hour = {m + (sideW - tapeW) / 2, L.adi.y, tapeW, a};
    L.minute = {w - m - sideW + (sideW - tapeW) / 2, L.adi.y, tapeW, a};

    const int32_t cornerY = L.adi.y + a + m;
    const int32_t cornerH = h - cornerY - m;
    L.zulu = {m, cornerY, sideW, cornerH};
    L.local = {w - m - sideW, cornerY, sideW, cornerH};
    return L;
}

PfdLayout PfdLayout::compute(int32_t w, int32_t h, int32_t cornerR)
{
    const float s = float(w < h ? w : h) / 240.0f;
    return h > w ? portrait(w, h, s, cornerR) : landscape(w, h, s);
}

} // namespace core
