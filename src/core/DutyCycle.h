// AeroDeck core: rolling-window transmit budget (S9 data-model §5, research R4, FR-003). Pure, host-tested.
// Records (start UTC ms, airtime ms) in a ring; a send is allowed when the airtime in the last window plus the new
// one stays within the budget. A full ring whose oldest record still counts refuses (conservative).
// save()/restore() let the service keep the history in RTC memory across eject and soft reset.
#pragma once
#include <cstddef>
#include <cstdint>

namespace core {

class DutyCycle {
public:
    static constexpr int kMaxRecords = 256;
    static constexpr size_t kRecSize = 10;   // int64 time + uint16 airtime
    static constexpr size_t kBlobMax = 4 + kMaxRecords * kRecSize + 2;

    explicit DutyCycle(uint32_t windowMs = 3600000, uint32_t budgetMs = 36000);

    bool canSend(int64_t nowUtcMs, uint32_t airMs) const;
    void record(int64_t nowUtcMs, uint32_t airMs);
    uint32_t usedMs(int64_t nowUtcMs) const;
    int64_t nextAllowedMs(int64_t nowUtcMs, uint32_t airMs) const;   // earliest time canSend becomes true
    uint32_t budgetMs() const { return budget_; }
    void setBudgetMs(uint32_t b) { budget_ = b; }
    int count() const { return n_; }

    size_t save(uint8_t* out, size_t cap) const;                       // 0 if cap is too small
    bool restore(const uint8_t* in, size_t n, int64_t nowUtcMs);       // false (and empty) on bad magic/CRC/length

private:
    struct Rec {
        int64_t t;
        uint16_t air;
    };
    const Rec& at(int i) const { return r_[(head_ + i) % kMaxRecords]; }   // 0 = oldest
    bool live(const Rec& r, int64_t now) const { return now - r.t < int64_t(window_); }
    void clear() { head_ = 0; n_ = 0; }

    uint32_t window_, budget_;
    Rec r_[kMaxRecords];
    int head_ = 0, n_ = 0;
};

} // namespace core
