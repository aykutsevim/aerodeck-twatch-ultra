#include "core/FlightMath.h"
#include <cstdio>

namespace core {
namespace FlightMath {

double nm(uint32_t steps, uint8_t strideCm) { return double(steps) * strideCm / 185200.0; }

void formatNm(double v, bool ok, char* out, size_t n)
{
    if (!ok) {
        snprintf(out, n, "--");
    } else if (v < 9.995) {
        snprintf(out, n, "%.2f", v < 0 ? 0.0 : v);
    } else {
        snprintf(out, n, "%.1f", v);
    }
}

uint16_t toCentiG(float g, float rangeG, bool& saturated)
{
    saturated = false;
    if (!(g > 0.0f)) {
        return 0;
    }
    if (rangeG > 0.0f && g >= rangeG * 0.999f) {
        saturated = true;
        g = rangeG;
    }
    const float c = g * 100.0f + 0.5f;
    return c > 65535.0f ? uint16_t(65535) : uint16_t(c);
}

void formatG(uint16_t gc, bool ok, bool saturated, char* out, size_t n)
{
    if (!ok) {
        snprintf(out, n, "--");
        return;
    }
    const unsigned tenths = (unsigned(gc) + 5u) / 10u;
    snprintf(out, n, "%u.%u%s", tenths / 10u, tenths % 10u, saturated ? "+" : "");
}

} // namespace FlightMath
} // namespace core
