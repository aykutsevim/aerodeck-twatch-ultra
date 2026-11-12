// AeroDeck core: Flight Miles and g-force arithmetic and display formats (S6 data-model §4). Pure, host-tested.
#pragma once
#include <cstddef>
#include <cstdint>

namespace core {
namespace FlightMath {

double nm(uint32_t steps, uint8_t strideCm);                      // steps × stride / 1852 m
void formatNm(double nm, bool ok, char* out, size_t n);           // "--", "0.00", "3.42", "12.4"
uint16_t toCentiG(float g, float rangeG, bool& saturated);        // clamped to rangeG (if > 0)
void formatG(uint16_t gc, bool ok, bool saturated, char* out, size_t n);   // "--", "1.0", "2.7", "16.0+"

} // namespace FlightMath
} // namespace core
