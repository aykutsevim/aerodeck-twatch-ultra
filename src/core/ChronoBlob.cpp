#include "core/ChronoBlob.h"
#include "core/RecBlob.h"

namespace core {
namespace ChronoBlob {

namespace {

void put(uint8_t*& p, uint64_t v, int n)
{
    for (int i = 0; i < n; i++) {
        *p++ = uint8_t(v >> (8 * i));
    }
}

uint64_t get(const uint8_t*& p, int n)
{
    uint64_t v = 0;
    for (int i = 0; i < n; i++) {
        v |= uint64_t(*p++) << (8 * i);
    }
    return v;
}

} // namespace

size_t encode(const ChronoModel::Raw& r, uint8_t* out)
{
    uint8_t* p = out;
    put(p, kVersion, 1);
    put(p, uint8_t(r.mode), 1);
    put(p, uint8_t(r.state), 1);
    put(p, uint8_t(r.phase), 1);
    put(p, r.cycles, 2);
    put(p, r.late ? 1 : 0, 1);
    put(p, 0, 1);
    put(p, uint32_t(r.intervalMs), 4);
    put(p, uint64_t(r.endUtcMs), 8);
    put(p, uint32_t(r.remainingMs), 4);
    put(p, uint32_t(r.countdownMs), 4);
    put(p, uint32_t(r.workMs), 4);
    put(p, uint32_t(r.breakMs), 4);
    const uint16_t crc = RecBlob::crc16(out, size_t(p - out));
    put(p, crc, 2);
    return size_t(p - out);
}

bool decode(const uint8_t* buf, size_t len, ChronoModel::Raw& out)
{
    if (!buf || len != kSize || buf[0] != kVersion) {
        return false;
    }
    const uint16_t crc = uint16_t(buf[kSize - 2] | (buf[kSize - 1] << 8));
    if (crc != RecBlob::crc16(buf, kSize - 2)) {
        return false;
    }
    const uint8_t* p = buf + 1;
    ChronoModel::Raw r{};
    const uint8_t mode = uint8_t(get(p, 1)), state = uint8_t(get(p, 1)), phase = uint8_t(get(p, 1));
    if (mode > 1 || state > 3 || phase > 2) {
        return false;
    }
    r.mode = ChronoModel::Mode(mode);
    r.state = ChronoModel::State(state);
    r.phase = ChronoModel::Phase(phase);
    r.cycles = uint16_t(get(p, 2));
    r.late = get(p, 1) != 0;
    get(p, 1);
    r.intervalMs = int64_t(uint32_t(get(p, 4)));
    r.endUtcMs = int64_t(get(p, 8));
    r.remainingMs = int64_t(uint32_t(get(p, 4)));
    r.countdownMs = int64_t(uint32_t(get(p, 4)));
    r.workMs = int64_t(uint32_t(get(p, 4)));
    r.breakMs = int64_t(uint32_t(get(p, 4)));
    out = r;
    return true;
}

} // namespace ChronoBlob
} // namespace core
