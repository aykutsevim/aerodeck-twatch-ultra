// AeroDeck core: an 8-bit alpha mask image (feature 011). Shared by the generated assets (src/assets/),
// IBoard (the boot splash), core::SplashCompose and the PFD emblem. Data only.
#pragma once
#include <cstdint>

struct AlphaArt {
    uint16_t w;
    uint16_t h;
    const uint8_t* a8;   // w*h bytes, row-major; 0 = black/transparent, 255 = full colour
};
