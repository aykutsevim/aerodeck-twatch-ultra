// Phone setup hotspot + captive portal (S2, FR-004a–c, contracts/setup_portal.md). Runs inside the net task only.
#pragma once
#include <cstddef>
#include <cstdint>
#include "services/NetService.h"

namespace SetupPortal {
constexpr uint32_t kSessionS = 300;   // FR-004a: auto-close after 5 minutes
using Progress = void (*)(const char* ssid, const char* code, uint16_t remainingS);
// Blocks until credentials are saved (Ok), stop is set (Cancelled) or the session expires (Timeout).
// On Ok, ssid/psk hold the new credentials. The access code is reported only through `progress` (shown on the watch).
NetResult run(volatile bool& stop, Progress progress, char* ssid, size_t ns, char* psk, size_t np);
} // namespace SetupPortal
