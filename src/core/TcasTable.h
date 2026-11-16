// AeroDeck core: TCAS contact table (S9 data-model §6, research R5, AD-SAS-001 §7.3). Pure, host-tested.
// Up to kMax contacts keyed by node id. RSSI is smoothed with an EMA (α 0.3) and mapped to 4 rings
// (0 = inner): ring = clamp((-40 - rssi) / 15, 0, 3). The bearing is a stable hash of the node id (cosmetic until
// GNSS, S12). enteredInner fires once per entry into ring 0.
#pragma once
#include <cstdint>
#include "core/BeaconCodec.h"

namespace core {

struct Contact {
    uint32_t nodeId;
    char callsign[9];
    float rssiEma;
    int16_t lastRssi;
    int8_t lastSnrQ4;   // SNR × 4 (dB)
    uint8_t battPct;
    uint32_t lastSeenMs;
    uint8_t ring;
    uint16_t bearingDeg;
    bool inInner;
    bool simulated;
};

class TcasTable {
public:
    static constexpr int kMax = 16;
    static constexpr float kAlpha = 0.3f;
    static constexpr int kRings = 4;

    // Returns the slot index. enteredInner = this update moved the contact into ring 0.
    int update(const Beacon& b, int16_t rssi, int8_t snrQ4, uint32_t nowMs, bool simulated, bool& enteredInner);
    // Removes contacts not heard for timeoutMs; their ids go to lost[] (up to maxLost). Returns the number removed.
    int age(uint32_t nowMs, uint32_t timeoutMs, uint32_t* lost, int maxLost);
    int removeSimulated();
    void clear() { n_ = 0; }

    int count() const { return n_; }
    const Contact& at(int i) const { return c_[i]; }
    int find(uint32_t nodeId) const;
    bool anyInner() const;

    static uint8_t ringFor(float rssi);
    static uint16_t bearingFor(uint32_t nodeId);

private:
    void removeAt(int i);
    Contact c_[kMax];
    int n_ = 0;
};

} // namespace core
