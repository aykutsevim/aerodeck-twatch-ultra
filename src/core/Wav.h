// AeroDeck core: WAV header parser (S5 data-model §2). Pure, host-tested.
// Accepts RIFF/WAVE, PCM (format 1), 16-bit, mono, 8–48 kHz; unknown chunks (LIST, ...) are skipped.
#pragma once
#include <cstddef>
#include <cstdint>

namespace core {
namespace Wav {

constexpr size_t kHeaderMax = 256;   // bytes to read before parse()

struct Info {
    uint32_t rate = 0;
    uint16_t channels = 0;
    uint16_t bits = 0;
    uint32_t dataOffset = 0;   // file offset of the first sample
    uint32_t dataBytes = 0;
};

bool parse(const uint8_t* buf, size_t len, Info& out);
uint32_t durationMs(const Info& w);

} // namespace Wav
} // namespace core
