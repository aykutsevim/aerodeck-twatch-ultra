// AeroDeck core: PFD layout boxes for any screen size (FR-018, data-model §9). Host-tested.
#pragma once
#include <cstdint>

namespace core {

struct Box {
    int32_t x, y, w, h;
};

struct PfdLayout {
    Box heading;   // top, above the ADI
    Box adi;       // square, centred
    Box hour;      // left tape
    Box minute;    // right tape
    Box zulu;      // bottom-left corner
    Box local;     // bottom-right corner
    Box eicas;     // bottom strip under the ADI
    Box maxg;      // top-left corner
    Box nm;        // top-right corner

    // cornerR: radius of the screen's rounded corners (0 = square); every box is kept inside the visible area.
    static PfdLayout compute(int32_t w, int32_t h, int32_t cornerR = 0);
};

// True if the whole box lies inside a w×h screen whose corners are rounded with radius r.
bool insideRounded(const Box& b, int32_t w, int32_t h, int32_t r);
// Smallest y at which a box starting at x (width bw) clears the top rounded corners of radius r.
int32_t minTopForRounded(int32_t x, int32_t bw, int32_t w, int32_t r);

} // namespace core
