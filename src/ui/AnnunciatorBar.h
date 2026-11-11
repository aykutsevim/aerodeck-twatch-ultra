// Master annunciator band (S4, contracts/annunciator_ui.md): replaces the S0 caution banner on the top layer.
// Shows the master alert in its level colour with a count, flashes while unacknowledged, cycles when acknowledged,
// and a tap acknowledges all. UI task only.
#pragma once

namespace AnnunciatorBar {
void begin();
void update();   // poll the AlertManager snapshot (cheap; call from the UI loop)
} // namespace AnnunciatorBar
