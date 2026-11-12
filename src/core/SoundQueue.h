// AeroDeck core: bounded priority queue of sound requests (S5 data-model §4, FR-002). Pure, host-tested.
// Highest priority first, FIFO within a priority; the same clip at the same priority is not queued twice;
// stale requests are dropped on pop (chime 2 s, others 5 s).
#pragma once
#include <cstddef>
#include <cstdint>
#include "core/Alerts.h"

namespace core {

enum class SoundPrio : uint8_t { None = 0, Sample = 1, Chime = 2, Caution = 3, Warning = 4 };

struct SoundReq {
    Clip clip;
    SoundPrio prio;
    uint32_t atMs;   // millis() when requested
};

enum class PushResult : uint8_t { Queued, Duplicate, Full };

uint32_t maxAgeMs(SoundPrio p);
bool shouldPreempt(SoundPrio playing, SoundPrio incoming);   // incoming > playing

class SoundQueue {
public:
    static constexpr size_t kCap = 6;

    PushResult push(const SoundReq& r);
    bool pop(uint32_t nowMs, SoundReq& out);   // false when empty (after dropping stale entries)
    void purge(SoundPrio minPrio);             // remove every entry with prio >= minPrio
    void clear() { n_ = 0; }
    size_t size() const { return n_; }

private:
    void removeAt(size_t i);

    SoundReq q_[kCap] = {};
    size_t n_ = 0;
};

} // namespace core
