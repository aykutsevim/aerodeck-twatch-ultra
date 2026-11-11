// Full-screen overlay shown while the phone setup hotspot is active (S2, US1 scenarios 6–9). UI task only.
// Shows a QR code (WIFI:T:WPA;S:<ssid>;P:<code>;;), the hotspot name, the one-time code, the page address,
// a countdown and Cancel. Shows and hides itself from the NetService status snapshot.
#pragma once
#include "services/NetService.h"

namespace NetSetupOverlay {
void begin();
void update(const NetStatus& st);   // call on NetStatusChanged
} // namespace NetSetupOverlay
