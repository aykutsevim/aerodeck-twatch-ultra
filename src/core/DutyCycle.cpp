#include "core/DutyCycle.h"
#include "core/RecBlob.h"

namespace core {

namespace {
constexpr uint8_t kMagic0 = 'D', kMagic1 = 'C';
}

DutyCycle::DutyCycle(uint32_t windowMs, uint32_t budgetMs) : window_(windowMs), budget_(budgetMs) {}

uint32_t DutyCycle::usedMs(int64_t now) const
{
    uint32_t sum = 0;
    for (int i = 0; i < n_; i++) {
        if (live(at(i), now)) {
            sum += at(i).air;
        }
    }
    return sum;
}

bool DutyCycle::canSend(int64_t now, uint32_t airMs) const
{
    if (n_ == kMaxRecords && live(at(0), now)) {
        return false;   // the ring is full and its oldest record still counts
    }
    return uint64_t(usedMs(now)) + airMs <= budget_;
}

void DutyCycle::record(int64_t now, uint32_t airMs)
{
    if (n_ > 0 && now < at(n_ - 1).t) {
        now = at(n_ - 1).t;   // keep the ring chronological
    }
    const uint16_t a = airMs > 0xFFFF ? 0xFFFF : uint16_t(airMs);
    if (n_ < kMaxRecords) {
        r_[(head_ + n_) % kMaxRecords] = {now, a};
        n_++;
    } else {
        r_[head_] = {now, a};
        head_ = (head_ + 1) % kMaxRecords;
    }
}

int64_t DutyCycle::nextAllowedMs(int64_t now, uint32_t airMs) const
{
    if (canSend(now, airMs)) {
        return now;
    }
    // Drop live records oldest-first until the remainder plus airMs fits; that record's expiry is the answer.
    uint32_t used = usedMs(now);
    for (int i = 0; i < n_; i++) {
        const Rec& r = at(i);
        if (!live(r, now)) {
            continue;
        }
        used -= r.air;
        // Once this record expires, the ring has room too (it was the oldest still live).
        if (uint64_t(used) + airMs <= budget_) {
            return r.t + int64_t(window_);
        }
    }
    return now + int64_t(window_);
}

size_t DutyCycle::save(uint8_t* out, size_t cap) const
{
    const size_t need = 4 + size_t(n_) * kRecSize + 2;
    if (!out || cap < need) {
        return 0;
    }
    out[0] = kMagic0;
    out[1] = kMagic1;
    out[2] = uint8_t(n_);
    out[3] = uint8_t(n_ >> 8);
    uint8_t* p = out + 4;
    for (int i = 0; i < n_; i++) {
        const uint64_t t = uint64_t(at(i).t);
        for (int b = 0; b < 8; b++) {
            *p++ = uint8_t(t >> (8 * b));
        }
        *p++ = uint8_t(at(i).air);
        *p++ = uint8_t(at(i).air >> 8);
    }
    const uint16_t crc = RecBlob::crc16(out, need - 2);
    *p++ = uint8_t(crc >> 8);
    *p = uint8_t(crc);
    return need;
}

bool DutyCycle::restore(const uint8_t* in, size_t n, int64_t now)
{
    clear();
    if (!in || n < 6 || in[0] != kMagic0 || in[1] != kMagic1) {
        return false;
    }
    const int cnt = int(in[2]) | (int(in[3]) << 8);
    const size_t need = 4 + size_t(cnt) * kRecSize + 2;
    if (cnt > kMaxRecords || n < need) {
        return false;
    }
    const uint16_t crc = uint16_t((in[need - 2] << 8) | in[need - 1]);
    if (crc != RecBlob::crc16(in, need - 2)) {
        return false;
    }
    const uint8_t* p = in + 4;
    for (int i = 0; i < cnt; i++) {
        uint64_t t = 0;
        for (int b = 0; b < 8; b++) {
            t |= uint64_t(*p++) << (8 * b);
        }
        uint16_t a = uint16_t(p[0] | (p[1] << 8));
        p += 2;
        int64_t ts = int64_t(t);
        if (ts > now) {
            ts = now;   // dated in the future (clock set back): count it from now
        }
        record(ts, a);
    }
    return true;
}

} // namespace core
