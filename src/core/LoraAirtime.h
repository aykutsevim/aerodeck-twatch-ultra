// AeroDeck core: LoRa time on air (Semtech AN1200.13 / SX126x datasheet §6.1.4, S9 data-model §4). Pure.
#pragma once
#include <cstddef>
#include <cstdint>

namespace core {
namespace LoraAirtime {

// sf 7..12, bwKHz 125/250/500, crDenom 5..8 (4/5..4/8). Low-data-rate optimisation when Tsym ≥ 16 ms.
uint32_t airtimeMs(uint8_t sf, float bwKHz, uint8_t crDenom, uint16_t preamble, size_t payload, bool explicitHeader,
                   bool crc);

} // namespace LoraAirtime
} // namespace core
