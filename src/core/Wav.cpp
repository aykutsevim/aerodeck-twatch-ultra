#include "core/Wav.h"
#include <cstring>

namespace core {
namespace Wav {

namespace {

uint32_t rd32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
uint16_t rd16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }

} // namespace

bool parse(const uint8_t* buf, size_t len, Info& out)
{
    if (!buf || len < 12 || memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0) {
        return false;
    }
    const uint64_t fileSize = uint64_t(rd32(buf + 4)) + 8;
    bool haveFmt = false;
    Info w;
    size_t pos = 12;
    while (pos + 8 <= len) {
        const uint8_t* c = buf + pos;
        const uint32_t size = rd32(c + 4);
        if (memcmp(c, "fmt ", 4) == 0) {
            if (size < 16 || pos + 8 + 16 > len) {
                return false;
            }
            const uint16_t format = rd16(c + 8);
            w.channels = rd16(c + 10);
            w.rate = rd32(c + 12);
            w.bits = rd16(c + 22);
            if (format != 1 || w.channels != 1 || w.bits != 16 || w.rate < 8000 || w.rate > 48000) {
                return false;
            }
            haveFmt = true;
        } else if (memcmp(c, "data", 4) == 0) {
            if (!haveFmt) {
                return false;
            }
            w.dataOffset = uint32_t(pos + 8);
            w.dataBytes = size;
            if (uint64_t(w.dataOffset) + w.dataBytes > fileSize) {
                return false;
            }
            out = w;
            return true;
        }
        pos += 8 + size + (size & 1u);   // chunks are word-aligned
    }
    return false;   // no data chunk within the header bytes
}

uint32_t durationMs(const Info& w)
{
    if (w.rate == 0 || w.channels == 0 || w.bits == 0) {
        return 0;
    }
    const uint64_t samples = w.dataBytes / (uint32_t(w.channels) * (w.bits / 8u));
    return uint32_t(samples * 1000u / w.rate);
}

} // namespace Wav
} // namespace core
