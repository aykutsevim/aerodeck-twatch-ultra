// AeroDeck power: reference-counted rail leases (NFR-PWR-001) and 1 Hz PMU snapshot.
#pragma once
#include "core/Types.h"
#include "services/Snapshot.h"

struct PowerSnapshot {
    core::PmuReading pmu;
    bool valid;
    uint32_t atMs;
};

class RailLease {
public:
    explicit RailLease(core::Rail r);
    ~RailLease();
    RailLease(RailLease&& o) noexcept;
    RailLease& operator=(RailLease&& o) noexcept;
    RailLease(const RailLease&) = delete;
    RailLease& operator=(const RailLease&) = delete;

private:
    core::Rail rail_;
    bool held_;
};

namespace PowerService {
void begin();   // forces optional rails off; registers the svc tick
const Snapshot<PowerSnapshot>& power();
uint8_t refcount(core::Rail r);
// Re-applies OFF to every optional rail with no lease (e.g. after vendor init powered them).
void enforceIdleRails();
void tick(uint32_t nowMs);
} // namespace PowerService
