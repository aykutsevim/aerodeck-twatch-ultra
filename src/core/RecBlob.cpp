#include "core/RecBlob.h"

namespace core {
namespace RecBlob {

namespace {

void put32(uint8_t*& p, uint32_t v)
{
    for (int i = 0; i < 4; i++) *p++ = uint8_t(v >> (8 * i));
}
void put16(uint8_t*& p, uint16_t v)
{
    *p++ = uint8_t(v);
    *p++ = uint8_t(v >> 8);
}
uint32_t get32(const uint8_t*& p)
{
    uint32_t v = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    p += 4;
    return v;
}
uint16_t get16(const uint8_t*& p)
{
    uint16_t v = uint16_t(p[0] | p[1] << 8);
    p += 2;
    return v;
}

void putRec(uint8_t*& p, const DayRecord& r)
{
    put32(p, uint32_t(r.day));
    put32(p, r.steps);
    put16(p, r.maxGc);
    put16(p, uint16_t(r.maxGMin));
}

DayRecord getRec(const uint8_t*& p)
{
    DayRecord r;
    r.day = int32_t(get32(p));
    r.steps = get32(p);
    r.maxGc = get16(p);
    r.maxGMin = int16_t(get16(p));
    return r;
}

} // namespace

uint16_t crc16(const uint8_t* p, size_t n)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        crc ^= uint16_t(p[i]) << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? uint16_t((crc << 1) ^ 0x1021) : uint16_t(crc << 1);
        }
    }
    return crc;
}

size_t encode(const DayLog& log, uint8_t* out)
{
    uint8_t* p = out;
    *p++ = kVersion;
    *p++ = uint8_t(1 + DayLog::kHistory);
    putRec(p, log.today);
    for (const DayRecord& r : log.history) {
        putRec(p, r);
    }
    put16(p, crc16(out, size_t(p - out)));
    return size_t(p - out);
}

bool decode(const uint8_t* buf, size_t len, DayLog& out)
{
    if (!buf || len != kSize || buf[0] != kVersion || buf[1] != 1 + DayLog::kHistory) {
        return false;
    }
    const uint8_t* c = buf + kSize - 2;
    if (crc16(buf, kSize - 2) != get16(c)) {
        return false;
    }
    const uint8_t* p = buf + 2;
    DayLog l;
    l.today = getRec(p);
    for (DayRecord& r : l.history) {
        r = getRec(p);
    }
    out = l;
    return true;
}

} // namespace RecBlob
} // namespace core
