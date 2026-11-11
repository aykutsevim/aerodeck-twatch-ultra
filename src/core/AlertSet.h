// AeroDeck core: the active alert set with deterministic ordering and acknowledge rules (S4 data-model §2).
// Pure, not thread-safe (AlertManager guards it). Host-tested.
#pragma once
#include <cstddef>
#include <cstdint>
#include "core/Alerts.h"

namespace core {

struct Alert {
    AlertId id;
    AlertLevel level;
    char text[25];
    bool acked;
    uint32_t raisedSeq;
};

// Added: new entry. Same: nothing changed. Updated: text changed or level lowered (ack kept, no pattern).
// Rearmed: level raised (unacknowledged, pattern plays). Full: no room.
enum class RaiseResult : uint8_t { Added, Updated, Rearmed, Same, Full };

class AlertSet {
public:
    static constexpr size_t kCap = 24;
    RaiseResult raise(AlertId id, AlertLevel level, const char* text);
    bool clear(AlertId id);
    uint8_t ackAll();
    bool ack(AlertId id);
    bool contains(AlertId id) const;
    bool anyUnacked() const;
    bool anyUnackedWarning() const;
    size_t sorted(Alert* out, size_t max) const;   // level desc, raisedSeq asc
    size_t count() const { return n_; }

private:
    int find(AlertId id) const;
    Alert a_[kCap] = {};
    size_t n_ = 0;
    uint32_t seq_ = 0;
};

} // namespace core
