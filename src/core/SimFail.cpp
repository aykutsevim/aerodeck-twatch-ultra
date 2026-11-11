#include "core/SimFail.h"
#include <cstring>

namespace core {
namespace SimFail {

struct Entry {
    Periph periph;
    const char* name;
};
static const Entry kTable[kBits] = {
    {Periph::Lora, "lora"}, {Periph::Nfc, "nfc"},         {Periph::Gnss, "gnss"}, {Periph::SdCard, "sd"},
    {Periph::Mic, "mic"},   {Periph::Haptics, "haptics"}, {Periph::Imu, "imu"},   {Periph::Rtc, "rtc"},
};

int bitFor(Periph p)
{
    for (int i = 0; i < kBits; i++) {
        if (kTable[i].periph == p) {
            return i;
        }
    }
    return -1;
}

Periph periphFor(int bit) { return (bit >= 0 && bit < kBits) ? kTable[bit].periph : Periph::Count; }

int bitForName(const char* name)
{
    if (!name) {
        return -1;
    }
    for (int i = 0; i < kBits; i++) {
        if (strcmp(kTable[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

const char* nameForBit(int bit) { return (bit >= 0 && bit < kBits) ? kTable[bit].name : nullptr; }

} // namespace SimFail
} // namespace core
