#include "core/LoraAirtime.h"
#include <cmath>

namespace core {
namespace LoraAirtime {

uint32_t airtimeMs(uint8_t sf, float bwKHz, uint8_t crDenom, uint16_t preamble, size_t payload, bool explicitHeader,
                   bool crc)
{
    if (bwKHz <= 0.0f || sf < 5 || sf > 12) {
        return 0;
    }
    const double tSym = double(1u << sf) / double(bwKHz);   // ms
    const int de = tSym >= 16.0 ? 1 : 0;
    const int ih = explicitHeader ? 0 : 1;
    const int cr = (crDenom < 5 ? 5 : crDenom > 8 ? 8 : crDenom) - 4;
    const double num = 8.0 * double(payload) - 4.0 * sf + 28.0 + 16.0 * (crc ? 1 : 0) - 20.0 * ih;
    const double den = 4.0 * (sf - 2 * de);
    double n = std::ceil(num / den) * (cr + 4);
    if (n < 0.0) {
        n = 0.0;
    }
    const double total = (double(preamble) + 4.25 + 8.0 + n) * tSym;
    return uint32_t(total + 0.5);
}

} // namespace LoraAirtime
} // namespace core
