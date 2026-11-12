#include "core/SoundQueue.h"

namespace core {

uint32_t maxAgeMs(SoundPrio p) { return p == SoundPrio::Chime ? 2000u : 5000u; }

bool shouldPreempt(SoundPrio playing, SoundPrio incoming) { return uint8_t(incoming) > uint8_t(playing); }

void SoundQueue::removeAt(size_t i)
{
    for (size_t j = i + 1; j < n_; j++) {
        q_[j - 1] = q_[j];
    }
    n_--;
}

PushResult SoundQueue::push(const SoundReq& r)
{
    for (size_t i = 0; i < n_; i++) {
        if (q_[i].clip == r.clip && q_[i].prio == r.prio) {
            return PushResult::Duplicate;
        }
    }
    if (n_ == kCap) {
        // evict the lowest priority (the newest of them) if the new request outranks it
        size_t low = 0;
        for (size_t i = 1; i < n_; i++) {
            if (uint8_t(q_[i].prio) <= uint8_t(q_[low].prio)) {
                low = i;
            }
        }
        if (uint8_t(r.prio) <= uint8_t(q_[low].prio)) {
            return PushResult::Full;
        }
        removeAt(low);
    }
    q_[n_++] = r;
    return PushResult::Queued;
}

bool SoundQueue::pop(uint32_t nowMs, SoundReq& out)
{
    for (size_t i = 0; i < n_;) {
        if (uint32_t(nowMs - q_[i].atMs) > maxAgeMs(q_[i].prio)) {
            removeAt(i);
        } else {
            i++;
        }
    }
    if (n_ == 0) {
        return false;
    }
    size_t best = 0;
    for (size_t i = 1; i < n_; i++) {
        if (uint8_t(q_[i].prio) > uint8_t(q_[best].prio)) {
            best = i;   // strictly greater keeps FIFO within a priority
        }
    }
    out = q_[best];
    removeAt(best);
    return true;
}

void SoundQueue::purge(SoundPrio minPrio)
{
    for (size_t i = 0; i < n_;) {
        if (uint8_t(q_[i].prio) >= uint8_t(minPrio)) {
            removeAt(i);
        } else {
            i++;
        }
    }
}

} // namespace core
