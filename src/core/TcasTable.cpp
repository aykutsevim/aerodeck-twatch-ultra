#include "core/TcasTable.h"
#include <cmath>
#include <cstring>

namespace core {

uint8_t TcasTable::ringFor(float rssi)
{
    const float r = std::floor((-40.0f - rssi) / 15.0f);
    if (!(r > 0.0f)) {
        return 0;
    }
    return r >= float(kRings - 1) ? uint8_t(kRings - 1) : uint8_t(r);
}

uint16_t TcasTable::bearingFor(uint32_t id)
{
    // murmur3 finalizer: consecutive ids spread over the whole circle
    uint32_t h = id;
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return uint16_t(h % 360u);
}

int TcasTable::find(uint32_t nodeId) const
{
    for (int i = 0; i < n_; i++) {
        if (c_[i].nodeId == nodeId) {
            return i;
        }
    }
    return -1;
}

bool TcasTable::anyInner() const
{
    for (int i = 0; i < n_; i++) {
        if (c_[i].ring == 0) {
            return true;
        }
    }
    return false;
}

int TcasTable::update(const Beacon& b, int16_t rssi, int8_t snrQ4, uint32_t nowMs, bool simulated,
                      bool& enteredInner)
{
    enteredInner = false;
    int i = find(b.nodeId);
    if (i < 0) {
        if (n_ < kMax) {
            i = n_++;
        } else {
            i = 0;   // replace the contact heard longest ago
            for (int k = 1; k < n_; k++) {
                if (int32_t(c_[k].lastSeenMs - c_[i].lastSeenMs) < 0) {
                    i = k;
                }
            }
        }
        Contact& n = c_[i];
        memset(&n, 0, sizeof n);
        n.nodeId = b.nodeId;
        n.rssiEma = float(rssi);
        n.bearingDeg = bearingFor(b.nodeId);
    } else {
        c_[i].rssiEma += kAlpha * (float(rssi) - c_[i].rssiEma);
    }
    Contact& c = c_[i];
    memcpy(c.callsign, b.callsign, sizeof c.callsign);
    c.callsign[8] = '\0';
    c.lastRssi = rssi;
    c.lastSnrQ4 = snrQ4;
    c.battPct = b.battPct;
    c.lastSeenMs = nowMs;
    c.simulated = simulated;
    c.ring = ringFor(c.rssiEma);
    const bool inner = c.ring == 0;
    enteredInner = inner && !c.inInner;
    c.inInner = inner;
    return i;
}

void TcasTable::removeAt(int i)
{
    for (int k = i; k + 1 < n_; k++) {
        c_[k] = c_[k + 1];
    }
    n_--;
}

int TcasTable::age(uint32_t nowMs, uint32_t timeoutMs, uint32_t* lost, int maxLost)
{
    int removed = 0;
    for (int i = 0; i < n_;) {
        if (nowMs - c_[i].lastSeenMs >= timeoutMs) {
            if (lost && removed < maxLost) {
                lost[removed] = c_[i].nodeId;
            }
            removed++;
            removeAt(i);
        } else {
            i++;
        }
    }
    return removed;
}

int TcasTable::removeSimulated()
{
    int removed = 0;
    for (int i = 0; i < n_;) {
        if (c_[i].simulated) {
            removeAt(i);
            removed++;
        } else {
            i++;
        }
    }
    return removed;
}

} // namespace core
