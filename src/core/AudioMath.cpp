#include "core/AudioMath.h"

namespace core {
namespace AudioMath {

int16_t gainQ15(uint8_t step)
{
    static const int16_t kGain[kVolumeSteps] = {0, 9830, 16384, 23170, 32767};
    return step < kVolumeSteps ? kGain[step] : 0;
}

void applyGain(int16_t* s, size_t n, int16_t q15)
{
    if (!s || q15 >= 32767) {
        return;
    }
    for (size_t i = 0; i < n; i++) {
        int32_t v = (int32_t(s[i]) * q15) >> 15;
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        s[i] = int16_t(v);
    }
}

} // namespace AudioMath
} // namespace core
