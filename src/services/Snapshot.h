// AeroDeck Snapshot<T>: single-writer / multi-reader seqlock for small POD state (SAS §5.2).
#pragma once
#include <atomic>
#include <cstdint>

template <class T>
class Snapshot {
public:
    void write(const T& v)
    {
        seq_.fetch_add(1, std::memory_order_acq_rel);   // odd: write in progress
        data_ = v;
        seq_.fetch_add(1, std::memory_order_acq_rel);   // even: stable
    }

    T read() const
    {
        T out;
        uint32_t s1, s2;
        do {
            s1 = seq_.load(std::memory_order_acquire);
            out = data_;
            s2 = seq_.load(std::memory_order_acquire);
        } while ((s1 & 1u) || s1 != s2);
        return out;
    }

    uint32_t seq() const { return seq_.load(std::memory_order_acquire); }

private:
    std::atomic<uint32_t> seq_{0};
    T data_{};
};
