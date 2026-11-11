# ADR-012: The ESP32-S3 Wi-Fi/IP stack is a platform service

- **Status:** Accepted (slice S2, 2026-11-11)
- **Relates to:** constitution Principle II (layering) and III (power); AD-SAS-001 §4, §6

## Context
Principle II says pages and services must not use driver types directly; hardware access goes through `IBoard`.
S2 needs the SoC's Wi-Fi radio and IP stack: Arduino `WiFi`, `WiFiUDP`, `WebServer`, `DNSServer`. Unlike the PMU, IMU,
LoRa or display, this stack is part of the ESP32-S3 itself. It's identical on the T-Watch Ultra and the T-Watch S3,
and has no board-specific pins or chips. Wrapping it in `IBoard` would add an abstraction with exactly one
implementation (Principle VIII).

## Decision
- The SoC Wi-Fi/IP stack is treated as a **platform service**, not a board driver.
- Its headers (`WiFi.h`, `WiFiUdp.h`, `WebServer.h`, `DNSServer.h`) may be used **only in `src/services/`**
  (`NetService`, `SetupPortal`). They are never used in `src/core/`, `src/ui/` or `src/pages/`. UI code reads
  `Snapshot<NetStatus>` and posts commands.
- **Power still follows Principle III:** Wi-Fi is `core::Rail::Wifi`, switched only by `PowerService`.
  `NetService` registers a switch hook (`PowerService::setRailHook`), and every transaction holds a
  `RailLease(Rail::Wifi)`. Wi-Fi is forced off at boot with the other optional rails, and every change is logged
  (`rail wifi ON/OFF`).

## Consequences
- No `IBoard` change for S2. Both boards share the network code.
- The layering check (S0 T062 / S2 T037) also checks that these headers appear only in `src/services/`.
- If a future board has an external Wi-Fi chip, a board-level abstraction would be needed then (revisit this ADR).

## Alternatives
- Add Wi-Fi methods to `IBoard`: one implementation shared by every board, with no benefit today.
- Keep a private Wi-Fi refcount in NetService: rejected by `/speckit-analyze` (C1), since it bypasses `PowerService`.
