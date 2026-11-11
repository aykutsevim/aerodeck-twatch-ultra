// AeroDeck core: FUEL FLOW drain-rate estimate and LOW FUEL hysteresis (data-model §4, research R5).
#pragma once
#include <cstddef>
#include <cstdint>

namespace core {

class FuelFlow {
public:
    static constexpr size_t kCapacity = 30;   // 30 one-minute samples
    static constexpr uint32_t kMinSpanMin = 10;
    static constexpr int kJumpPct = 5;

    // One sample per minute while discharging. Charging clears history (no rate while refuelling).
    void addSample(uint32_t minute, uint8_t pct, bool charging);
    // Drain rate in %/h (positive = draining), or NaN when fewer than 10 minutes of history exist.
    float ratePctPerH() const;
    size_t count() const { return n_; }
    void reset() { n_ = 0; head_ = 0; }

private:
    struct Sample {
        uint32_t minute;
        uint8_t pct;
    };
    const Sample& at(size_t i) const { return ring_[(head_ + kCapacity - n_ + i) % kCapacity]; }
    Sample ring_[kCapacity] = {};
    size_t n_ = 0;
    size_t head_ = 0;   // next write position
};

class FuelState {
public:
    static constexpr uint8_t kLowBelow = 15;   // low when pct < 15
    static constexpr uint8_t kClearAbove = 17; // clears when pct > 17
    bool update(uint8_t pct, bool charging);
    bool low() const { return low_; }

private:
    bool low_ = false;
};

} // namespace core
