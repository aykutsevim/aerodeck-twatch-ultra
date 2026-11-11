#include "core/Ntp.h"
#include <cstring>

namespace core {
namespace Ntp {

static void put32(uint8_t* p, uint32_t v)
{
    p[0] = uint8_t(v >> 24);
    p[1] = uint8_t(v >> 16);
    p[2] = uint8_t(v >> 8);
    p[3] = uint8_t(v);
}

static uint32_t get32(const uint8_t* p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

int64_t ntpToUnix(uint32_t ntpSec)
{
    // RFC 4330 §3: values with the MSB clear are in era 1 (after 2036-02-07).
    int64_t s = int64_t(ntpSec);
    if ((ntpSec & 0x80000000u) == 0) {
        s += 0x100000000LL;
    }
    return s - kUnixToNtp;
}

// NTP 64-bit timestamp → Unix milliseconds.
static int64_t tsToMs(const uint8_t* p)
{
    const uint32_t sec = get32(p);
    const uint32_t frac = get32(p + 4);
    return ntpToUnix(sec) * 1000 + int64_t((uint64_t(frac) * 1000u) >> 32);
}

void buildRequest(uint8_t* buf, int64_t unixSec, uint16_t ms)
{
    memset(buf, 0, kPacketSize);
    buf[0] = 0x23;   // LI 0, VN 4, Mode 3
    put32(buf + 40, uint32_t(unixSec + kUnixToNtp));
    put32(buf + 44, uint32_t((uint64_t(ms) << 32) / 1000u));
}

bool parseResponse(const uint8_t* rsp, size_t len, const uint8_t* req, int64_t nowUnixSec, uint16_t nowMs, Result& out)
{
    if (!rsp || !req || len < kPacketSize) {
        return false;
    }
    const uint8_t li = rsp[0] >> 6;
    const uint8_t mode = rsp[0] & 0x07;
    const uint8_t stratum = rsp[1];
    if (mode != 4 || li == 3 || stratum < 1 || stratum > 15) {
        return false;
    }
    if (memcmp(rsp + 24, req + 40, 8) != 0) {   // originate must echo our transmit timestamp
        return false;
    }
    if (get32(rsp + 40) == 0 && get32(rsp + 44) == 0) {
        return false;
    }
    const int64_t t1 = tsToMs(req + 40);                  // client send
    const int64_t t2 = tsToMs(rsp + 32);                  // server receive
    const int64_t t3 = tsToMs(rsp + 40);                  // server send
    const int64_t t4 = nowUnixSec * 1000 + nowMs;         // client receive
    const int64_t offset = ((t2 - t1) + (t3 - t4)) / 2;
    const int64_t corrected = t4 + offset;
    out.offsetMs = offset;
    out.unixSec = corrected / 1000;
    out.ms = uint16_t(corrected % 1000);
    return true;
}

} // namespace Ntp
} // namespace core
