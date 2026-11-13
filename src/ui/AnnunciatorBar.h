// Master annunciator band (S4, contracts/annunciator_ui.md): replaces the S0 caution banner on the top layer.
// Shows the master alert in its level colour with a count, flashes while unacknowledged, cycles when acknowledged,
// and a tap acknowledges all. UI task only.
#pragma once
#include <cstdint>

namespace AnnunciatorBar {
void begin();
void update();   // poll the AlertManager snapshot (cheap; call from the UI loop)
// S7: vertical offset of the band (the watch face moves it below the Pull-to-Eject handle; research R8).
void setTopOffset(int32_t px);
// S7 (FR-015): NVG looks: WARNING inverse + 2 Hz blink, CAUTION inverse, ADVISORY green outline.
void setNvg(bool on);
} // namespace AnnunciatorBar
