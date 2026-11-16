#include "core/LoraRegion.h"

namespace core {
namespace LoraRegion {
namespace {

constexpr RegionLimits kRegions[kCount] = {
    {"EU868", 869.40f, 869.65f, 869.525f, 2, 14, 10, 0, {125.0f, 250.0f, 0.0f}, 2},
    {"US915", 902.3f, 914.9f, 903.9f, 2, 20, 10, 400, {125.0f, 250.0f, 500.0f}, 3},
};

} // namespace

const RegionLimits& limits(uint8_t region) { return kRegions[region < kCount ? region : 0]; }

LoraProfile resolve(uint8_t region, int powerDbm, int sf, int bwIdx, uint8_t& clamped)
{
    clamped = 0;
    if (region >= kCount) {
        clamped |= kClampRegion;
        region = 0;
    }
    const RegionLimits& r = kRegions[region];
    if (powerDbm < r.powerMin) {
        powerDbm = r.powerMin;
        clamped |= kClampPower;
    } else if (powerDbm > r.powerMax) {
        powerDbm = r.powerMax;
        clamped |= kClampPower;
    }
    if (sf < kSfMin) {
        sf = kSfMin;
        clamped |= kClampSf;
    } else if (sf > kSfMax) {
        sf = kSfMax;
        clamped |= kClampSf;
    }
    if (bwIdx < 0 || bwIdx >= r.bwCount) {
        bwIdx = 0;
        clamped |= kClampBw;
    }
    LoraProfile p{};
    p.freqMHz = r.freqDefaultMHz;
    p.bwKHz = r.bwKHz[bwIdx];
    p.sf = uint8_t(sf);
    p.crDenom = kCrDenom;
    p.powerDbm = int8_t(powerDbm);
    p.syncWord = kSyncWord;
    p.preamble = kPreamble;
    return p;
}

bool dwellOk(uint8_t region, uint32_t airMs)
{
    const RegionLimits& r = limits(region);
    return r.maxDwellMs == 0 || airMs <= r.maxDwellMs;
}

uint32_t budgetMs(uint8_t region) { return uint32_t(limits(region).dutyPerMille) * 3600u; }

} // namespace LoraRegion
} // namespace core
