# Implementation Plan: TCAS LoRa Radar (Slice S9)

**Branch**: `010-tcas-lora-radar` | **Date**: 2026-11-16 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/010-tcas-lora-radar/spec.md`

**Implements**:
- AD-SAS-001 §4: `IBoard` radio API.
- §6: `TcasService`, the `radio` task, `TcasSnapshot`.
- §7.2: beacon v1.
- §7.3: RSSI rings.
- §8: `TcasPage`.
- SRS FR-TCS-001…004/006/008, NFR-REG-001, NFR-SEC-003. Closes OI-6.

## Summary

- **Driver:** the SX1262 is driven through LilyGoLib's RadioLib `radio` object behind a small `IBoard` radio API.
  A dedicated `radio` task on core 0 handles DIO1-interrupt RX and transmits.
- **TcasService** (svc task):
  - holds `RailLease(Lora)` only while TCAS is on;
  - schedules beacons (30 s ±10 %, 15 s–5 min), gated by a pure **1 %/hour duty-cycle budget**;
  - keeps a 16-contact table (EMA RSSI → 4 rings, stable hashed bearing, 5 min age-out);
  - raises a **TRAFFIC** advisory (with the "Traffic" callout) on entry into the inner ring.
- **Region:** **EU868 869.525 MHz SF9/BW125/CR4/5 at 10 dBm** (2–14 dBm), legal in Türkiye, on the unit's
  915 MHz-tuned hardware (R1, reduced range, documented). US915 is selectable only where legal.
- **TcasPage:** a PPI with rings, a 20 Hz sweep, tappable bogeys and a detail card.
- **Verification with one device (R8b):**
  - transmit-done counters and a 1-hour duty soak;
  - the rail state;
  - simulated contacts;
  - a listen-only receive probe;
  - the power measurement.

## Technical Context

**Language/Version**: C++17 (xtensa GCC 8.4, Arduino-ESP32 2.0.17 / IDF 4.4). Host tests: WinLibs GCC.

**Primary Dependencies**:
- RadioLib (pinned in S0) through LilyGoLib's `radio` (SX1262), only in `src/board/`;
- LVGL 9.6;
- **no new libraries**.

**Storage**: settings schema **v10**:
- `tcas` (0), `callsign` (Str 1..8);
- `tcasint` (s, 15..300, 30), `tcasage` (s, 60..1800, 300);
- `lorareg` (0 EU868, 1 US915), `lorapwr` (dBm, 2..20, 10), `lorasf` (7..12, 9), `lorabw` (index, 0 = 125 kHz).

**Testing**:
- Unity on `env:native`:
  - `test_beacon_codec`: round trip, every reject, position variant;
  - `test_duty_cycle`: window sum, boundary, the 15 s soak simulation never over budget, US915 dwell;
  - `test_lora_airtime`: the Semtech formula vs. known values;
  - `test_tcas_table`: EMA, rings, bearing stability, ageing, the full table, the `enteredInner` re-arm;
  - `test_lora_region`: limits and clamps;
  - `test_settings` v10;
  - `test_alerts`: Traffic + the advisory callout rule.
- On-device quickstart Q0–Q7 with a single device.

**Target Platform**: T-Watch Ultra. The T-Watch S3 must compile: its radio API returns false, so the page shows
"RADIO N/A".

**Project Type**: Embedded firmware.

**Performance Goals**:
- beacon transmit ≤ 0.25 s;
- svc tick unaffected (transmits run on the radio task);
- PFD median ≥ 30 fps with TCAS on;
- PPI sweep at 20 Hz while visible.

**Constraints**:
- RadioLib and LilyGoLib only in `src/board/` (II);
- `RailLease(Lora)` only while on (III);
- region limits and duty budget enforced before every transmit (VI);
- the beacon has no personal data beyond the callsign (VI);
- SPI shared with SD/NFC under `lockSPI`.

**Scale/Scope**:
- 5 pure core modules (`BeaconCodec`, `DutyCycle`, `LoraAirtime`, `TcasTable`, `LoraRegion`) + 1 alert id;
- `IBoard` +6 methods;
- 1 service + the radio task;
- 1 page;
- settings v10;
- about 8 console commands.

No NEEDS CLARIFICATION remains (research R1–R8b).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.* (Constitution v1.0.4)

| # | Principle | Pre | Post | Evidence |
| :-- | :--- | :---: | :---: | :--- |
| I | Every slice ships | ✅ | ✅ | TCAS is off by default. Radio failure → "RADIO FAIL" plus the existing peripheral caution (the S3 shows "RADIO N/A"); the rest of the watch is unaffected. The S3 compiles |
| II | Layered architecture | ✅ | ✅ | RadioLib/LilyGoLib only in `BoardUltra` behind the `IBoard` radio API. Pure codec, duty, airtime, table and region in core. The service talks to the page through `TcasSnapshot` and posted commands |
| III | Battery is a feature | ✅ | ✅ | `RailLease(Lora)` only while TCAS is on; rail off at eject (S7). **Duty cycle:** RX continuous while on; TX ≈ 185 ms per 30 s (≈ 0.6 %), capped at 1 %/h. Drain measured (FR-014); the total with TCAS on must stay ≤ 50 mA (NFR-PWR-002) or be documented as an exception. Off by default |
| IV | Frame time is sacred | ✅ | ✅ | Transmits on a separate core-0 task. PPI redraw only while visible. PFD fps measured with TCAS on |
| V | Test off-device first | ✅ | ✅ | All 5 core modules + settings + alerts are tested first. Device checks Q0–Q7 |
| VI | Deliberate, compliant, private | ✅ | ✅ | **Region-compliant:** EU868 in the legal band (869.525 MHz, ≤ 14 dBm) on 915-tuned hardware (R1), US915 only where legal. **Duty cycle enforced in software before every transmit** (1 %/h, stricter than g3's 10 %); the airtime history survives eject and soft reset (RTC memory, R4). The beacon has only version, id, callsign, seq and battery; no position. The receive probe is listen-only |
| VII | Reproducible, portable | ✅ | ✅ | No new libraries (RadioLib already pinned). The S3 compiles |
| VIII | Simplicity | ✅ | ✅ | No meshing, encryption or RX duty cycling. The Meshtastic decode is a hook only (FR-TCS-007 Could) |

**Gate: PASS.** Complexity Tracking is not needed.

**SRS amendments**:
- **OI-6 closed:** 915 MHz hardware; EU868 legal operation documented.
- FR-TCS-001 note: interval 15 s–5 min, ±10 % jitter.
- FR-TCS-006 note: TRAFFIC is the one speaking advisory.
- FR-TCS-008 note: the EU868 profile and power range; US915 dwell.
- NFR-REG-001 note: 1 %/h enforced; g3 allows 10 %.

**SAS notes**:
- §4 radio API;
- §6 the TcasService row + the `radio` task;
- §7.1 settings v10;
- §7.2 the beacon is 18 B without position;
- §8 the TCAS page in the ring;
- the ADR log: **ADR-015** "LoRa in EU868 on 915 MHz hardware; 1 % self-imposed duty budget".

## Project Structure

### Documentation (this feature)

```text
specs/010-tcas-lora-radar/
├── plan.md  research.md  data-model.md  quickstart.md
├── contracts/serial_commands.md  contracts/board_radio.md
├── checklists/requirements.md
└── tasks.md            # /speckit-tasks
```

### Source Code (new or changed)

```text
src/core/BeaconCodec.{h,cpp}   DutyCycle.{h,cpp}   LoraAirtime.{h,cpp}   TcasTable.{h,cpp}   LoraRegion.{h,cpp}
src/core/Alerts.{h,cpp}        # + Traffic (44); calloutFor: an advisory speaks only with an explicit clip
src/core/ScreenPolicy.{h,cpp}  # onAlert: a speaking advisory may wake the screen (TRAFFIC)
src/services/AlertManager.cpp  # AlertRaised event: wake flag for speaking advisories
src/core/SettingDefs.{h,cpp}   # v10: tcas, callsign, tcasint, tcasage, lorareg, lorapwr, lorasf, lorabw
src/board/IBoard.h, BoardUltra.cpp, BoardS3.cpp   # radioStart/Stop/Send/Poll/Airtime + nodeId()
src/services/TcasService.{h,cpp}   # radio task, lease, scheduler, duty gate, table, alerts, snapshot, `ad tcas …`
src/ui/Page.h                      # PageId::Tcas
src/pages/TcasPage.{h,cpp}         # PPI + sweep + bogeys + card + toggle
src/app/main.cpp                   # begin/tick; register TcasPage
test/test_beacon_codec/ test_duty_cycle/ test_lora_airtime/ test_tcas_table/ test_lora_region/
docs/adr/015-lora-eu868-on-915-hardware.md, SRS, SAS, README, power_report.md
```

**Structure Decision**: Continue the S0–S8 layout.

## Implementation Notes (for /speckit-tasks)

1. **Order:**
   1. **Radio probe first:** `radioStart` with the EU868 profile under the lease. `ad probe s9 tx`:
      - send 3 packets: transmit-done, airtime vs `LoraAirtime`, no SPI conflicts with SD/NFC;
      - `ad probe s9 rx 60`: listen-only with the public sync word, counting headers and packets;
      - the rail on and off (`ad pwr log`).
   2. Core tests → core.
   3. Settings v10 + the alert.
   4. TcasService + the radio task + the console.
   5. Page.
   6. Soak + power measurement.
   7. Docs.
2. **The DIO1 ISR** only notifies the radio task (no SPI in the ISR).
3. **The S7 eject** powers the radio off (`POWER_RADIO`). On wake, `TcasService::begin` re-takes the lease if
   `tcas` = 1.
4. **Measure:**
   - airtime per beacon;
   - the 1-hour soak at the 5 s test interval (sent, delayed, skipped; window maximum ≤ 36 s);
   - screen-off current with TCAS on vs off (USB meter);
   - PFD fps with TCAS on.
5. **At the end:**
   - update `docs/product_roadmap.md` (S9 → ✅ with the pending two-device checks);
   - update the risks table: "915 MHz hardware in an 868 region: reduced range".

## Complexity Tracking

Not needed. The Constitution Check passed with no violations.
