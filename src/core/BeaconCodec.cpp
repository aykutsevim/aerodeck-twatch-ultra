#include "core/BeaconCodec.h"
#include <cstring>

namespace core {
namespace BeaconCodec {
namespace {

char upper(char c) { return (c >= 'a' && c <= 'z') ? char(c - 32) : c; }

bool callChar(char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-'; }

void putI24(uint8_t* p, int32_t v)
{
    const uint32_t u = uint32_t(v);
    p[0] = uint8_t(u);
    p[1] = uint8_t(u >> 8);
    p[2] = uint8_t(u >> 16);
}

int32_t getI24(const uint8_t* p)
{
    uint32_t u = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16);
    if (u & 0x800000u) {
        u |= 0xFF000000u;
    }
    return int32_t(u);
}

} // namespace

size_t encode(const Beacon& b, uint8_t* out)
{
    out[0] = kMagic;
    out[1] = kVersion;
    out[2] = kTypeBeacon;
    out[3] = uint8_t(b.nodeId);
    out[4] = uint8_t(b.nodeId >> 8);
    out[5] = uint8_t(b.nodeId >> 16);
    out[6] = uint8_t(b.nodeId >> 24);
    out[7] = b.seq;
    bool end = false;
    for (int i = 0; i < 8; i++) {
        const char c = end ? '\0' : b.callsign[i];
        if (c == '\0') {
            end = true;
        }
        out[8 + i] = uint8_t(end ? 0 : upper(c));   // zero-padded (SAS 7.2)
    }
    out[16] = b.battPct > 100 ? 100 : b.battPct;
    out[17] = b.hasPos ? kFlagPos : 0;
    if (!b.hasPos) {
        return kSize;
    }
    putI24(out + 18, b.lat24);
    putI24(out + 21, b.lon24);
    return kSizePos;
}

bool decode(const uint8_t* buf, size_t n, Beacon& out)
{
    if (!buf || (n != kSize && n != kSizePos)) {
        return false;
    }
    if (buf[0] != kMagic || buf[1] != kVersion || buf[2] != kTypeBeacon) {
        return false;
    }
    const bool pos = (buf[17] & kFlagPos) != 0;
    if (pos != (n == kSizePos) || buf[16] > 100) {
        return false;
    }
    char call[9];
    int len = 0;
    bool padding = false;
    for (int i = 0; i < 8; i++) {
        const char c = upper(char(buf[8 + i]));
        if (c == 0 || c == ' ') {   // padding: NUL (SAS) or space (tolerated)
            padding = true;
            continue;
        }
        if (padding || !callChar(c)) {   // a space inside the callsign or a bad character
            return false;
        }
        call[len++] = c;
    }
    if (len == 0) {
        return false;
    }
    call[len] = '\0';
    Beacon b{};
    b.nodeId = uint32_t(buf[3]) | (uint32_t(buf[4]) << 8) | (uint32_t(buf[5]) << 16) | (uint32_t(buf[6]) << 24);
    b.seq = buf[7];
    memcpy(b.callsign, call, sizeof call);
    b.battPct = buf[16];
    b.hasPos = pos;
    if (pos) {
        b.lat24 = getI24(buf + 18);
        b.lon24 = getI24(buf + 21);
    }
    out = b;
    return true;
}

bool isMeshtastic(const uint8_t* buf, size_t n)
{
    (void)buf;
    (void)n;
    return false;
}

} // namespace BeaconCodec
} // namespace core
