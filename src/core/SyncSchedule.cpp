#include "core/SyncSchedule.h"

namespace core {

void SyncSchedule::onBoot(uint32_t now, bool configured, uint16_t intervalMin)
{
    interval_ = intervalMin ? intervalMin : 60;
    failures_ = 0;
    base_ = now;
    armed_ = configured;
    next_ = now;   // due immediately if configured
}

void SyncSchedule::onSuccess(uint32_t now)
{
    failures_ = 0;
    base_ = now;
    armed_ = true;
    next_ = now + intervalMs();
}

void SyncSchedule::onFailure(uint32_t now)
{
    static const uint32_t kBackoffMin[] = {2, 5, 15};
    if (failures_ < 255) {
        failures_++;
    }
    uint32_t step = kBackoffMin[(failures_ < 3 ? failures_ : 3) - 1] * 60000u;
    if (step > intervalMs()) {
        step = intervalMs();
    }
    armed_ = true;
    next_ = now + step;
}

void SyncSchedule::onConfigured(uint32_t now)
{
    failures_ = 0;
    armed_ = true;
    next_ = now;
}

void SyncSchedule::setInterval(uint16_t intervalMin)
{
    interval_ = intervalMin ? intervalMin : 60;
    if (failures_ == 0) {
        next_ = base_ + intervalMs();
    }
}

bool SyncSchedule::due(uint32_t now) const
{
    return armed_ && int32_t(now - next_) >= 0;
}

bool SyncSchedule::isUnsynced(int64_t nowUtc, int64_t lastSyncUtc, bool configured)
{
    if (!configured) {
        return false;
    }
    return lastSyncUtc <= 0 || nowUtc - lastSyncUtc > 86400;
}

} // namespace core
