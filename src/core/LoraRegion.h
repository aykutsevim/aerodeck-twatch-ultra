// AeroDeck core: legal LoRa limits per region (S9 data-model §2, research R1, ADR-015). Pure, host-tested.
// EU868 is narrowed to sub-band g3 (869.40–869.65 MHz) on one fixed channel; the 1 % duty budget is self-imposed
// (g3 allows 10 %). US915 is for places where that band is legal (FCC 400 ms dwell for non-hopping use).
#pragma once
#include <cstdint>

namespace core {

struct LoraProfile {
    float freqMHz;
    float bwKHz;
    uint8_t sf;
    uint8_t crDenom;   // 5 = 4/5
    int8_t powerDbm;
    uint8_t syncWord;
    uint16_t preamble;
};

struct RegionLimits {
    const char* name;
    float freqMinMHz, freqMaxMHz, freqDefaultMHz;
    int8_t powerMin, powerMax;
    uint16_t dutyPerMille;   // budget per rolling hour, ‰
    uint16_t maxDwellMs;     // 0 = no dwell limit
    float bwKHz[3];
    uint8_t bwCount;
};

namespace LoraRegion {

constexpr uint8_t kCount = 2;   // 0 EU868, 1 US915
constexpr uint8_t kSyncWord = 0x12;   // private LoRa sync word (RadioLib default)
constexpr uint16_t kPreamble = 8;
constexpr uint8_t kCrDenom = 5;
constexpr uint8_t kSfMin = 7, kSfMax = 12;

enum Clamp : uint8_t { kClampRegion = 1, kClampPower = 2, kClampSf = 4, kClampBw = 8 };

const RegionLimits& limits(uint8_t region);   // out of range → EU868
// Clamps every field to the region; `clamped` reports which ones (Clamp bits).
LoraProfile resolve(uint8_t region, int powerDbm, int sf, int bwIdx, uint8_t& clamped);
bool dwellOk(uint8_t region, uint32_t airMs);
uint32_t budgetMs(uint8_t region);           // per rolling hour

} // namespace LoraRegion
} // namespace core
