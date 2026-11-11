// AeroDeck core: when to sync time next (boot, interval, failure backoff) and staleness (data-model §5). Pure.
#pragma once
#include <cstdint>

namespace core {

class SyncSchedule {
public:
    void onBoot(uint32_t nowMs, bool configured, uint16_t intervalMin);
    void onSuccess(uint32_t nowMs);
    void onFailure(uint32_t nowMs);     // backoff 2, 5, 15 min, capped at the interval
    void onConfigured(uint32_t nowMs);  // new credentials: sync now
    void onForgotten() { armed_ = false; }
    void setInterval(uint16_t intervalMin);
    bool due(uint32_t nowMs) const;
    uint32_t nextMs() const { return next_; }

    // TIME UNSYNC: configured and never synced, or the last success is more than 24 h old.
    static bool isUnsynced(int64_t nowUtc, int64_t lastSyncUtc, bool configured);

private:
    uint32_t intervalMs() const { return uint32_t(interval_) * 60000u; }
    bool armed_ = false;
    uint16_t interval_ = 60;
    uint8_t failures_ = 0;
    uint32_t base_ = 0;   // time of the last success (or boot)
    uint32_t next_ = 0;
};

} // namespace core
