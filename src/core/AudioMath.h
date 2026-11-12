// AeroDeck core: volume steps and q15 gain (S5 data-model §3, FR-011). Pure, host-tested.
#pragma once
#include <cstddef>
#include <cstdint>

namespace core {
namespace AudioMath {

constexpr uint8_t kVolumeSteps = 5;   // 0 = Off, 1..4

// Steps 0..4 → {0, 0.30, 0.50, 0.71, 1.0} in q15 (≈ off / −10.5 / −6 / −3 / 0 dB; raised after the T016 listening
// check found step 2 at −9 dB too quiet). Out of range → 0.
int16_t gainQ15(uint8_t step);
// In place, saturating; q15 ≥ 32767 leaves the samples unchanged.
void applyGain(int16_t* s, size_t n, int16_t q15);

} // namespace AudioMath
} // namespace core
