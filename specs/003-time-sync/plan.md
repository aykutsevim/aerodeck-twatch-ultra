# Implementation Plan: Time Sync & Connectivity (Slice S2)

**Branch**: `003-time-sync` | **Date**: 2026-11-11 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/003-time-sync/spec.md`

**Implements**: AD-SAS-001 §5.1 (`net` task, core 0), §5.3 (hourly sync sequence; METAR is added in S3), §6 (TimeService,
NetService), §7.1 (settings). Builds on S1's TimeService (`set(utc)`, CLOCK NOT SET) and replaces S1's interim `tzoff`.

## Summary

The watch gets Wi-Fi credentials from a **phone setup hotspot** (captive portal with a QR code on the watch, a one-time access
code, 5-minute auto-close) or a **USB command**. It syncs UTC with a **minimal SNTP client** at boot and every
`syncmin` minutes (default 60), writes the RTC, and turns Wi-Fi off after every transaction. Local time follows
**named time zones with summer-time rules**, via a pure POSIX-TZ engine and a built-in zone table that replaces the
fixed offset. Sync status and a TIME UNSYNC advisory complete the slice. All blocking network work runs on a
dedicated core-0 `net` task; the UI only posts commands and reads snapshots.

## Technical Context

**Language/Version**: C++17 (xtensa GCC 8.4, Arduino-ESP32 2.0.17 / IDF 4.4). Host tests use WinLibs GCC 16.2.

**Primary Dependencies**: Framework built-ins only: `WiFi`, `WiFiUDP`, `WebServer`, `DNSServer`, `Preferences`. LVGL 9.6
`lv_qrcode` and `lv_roller` (both enabled in LilyGoLib's `lv_conf.h`). **No new libraries.**

**Storage**: NVS settings schema v3 (`wifissid`, `wifipsk` secret, `tzid`, `syncmin`, `lastsync`; `tzoff` removed by migration).

**Testing**: Unity on `env:native`: `core::Ntp`, `core::PosixTz`, `core::Zones`, `core::SyncSchedule`, quoted tokens in
`CommandParser`, settings v3 (types, secret, migration mapping). On-device quickstart Q2–Q8.

**Target Platform**: T-Watch Ultra (410×502). T-Watch S3 must compile (same Wi-Fi code; no board dependency).

**Project Type**: Embedded firmware.

**Performance Goals**: Boot sync ≤30 s; any transaction ≤15 s; PFD FPS during a sync ≥28 (S1 median 30 minus ≤2);
setup page usable on iOS/Android.

**Constraints**: Wi-Fi OFF outside transactions/setup (SC-003); internal heap ≥40 KB during Wi-Fi; secrets never
logged/displayed; no blocking on the UI task; hotspot ≤300 s.

**Scale/Scope**: 1 new task, 2 services (NetService new, TimeService extended), ~6 core modules, 1 portal page, Settings
section, ~10 serial commands.

No NEEDS CLARIFICATION remains (research R1–R9). Two items are **measured by design** (heap dip and FPS during Wi-Fi, R1).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.* (Constitution v1.0.2)

| # | Principle | Pre | Post | Evidence |
| :-- | :--- | :---: | :---: | :--- |
| I | Every slice ships | ✅ | ✅ | No network → RTC time as in S1; `ad time set` fallback kept (FR-012). Every network failure is bounded and reported, never fatal |
| II | Layered architecture | ✅ | ✅ | Network code in `services/NetService` and `services/SetupPortal` (+ the `net` task). NTP/TZ/schedule/zones are pure `core/`. UI reads `Snapshot<NetStatus>`. No LVGL off the UI task. **ADR-012**: the ESP32-S3's built-in Wi-Fi/IP stack (`WiFi`, `WiFiUDP`, `WebServer`, `DNSServer`) is a *platform service* identical on every board, not a board driver. It's allowed in `src/services/` only (never in `core/`, `ui/`, `pages/`), so no `IBoard` wrapper is needed. Wi-Fi power still goes through `PowerService` (III) |
| III | Battery is a feature | ✅ | ✅ | Wi-Fi is a `PowerService` rail (`Rail::Wifi`, `RailLease`, switch hook registered by NetService). It's forced OFF at boot and OFF after every transaction (logged `rail wifi OFF`, SC-003). Duty cycle: one ≤15 s transaction per `syncmin` (default 60 min ≈ 0.4 %) |
| IV | Frame time is sacred | ✅ | ✅ | All blocking I/O on the core-0 `net` task. Wi-Fi driver pinned to core 0 (sdkconfig). FPS and heap measured during a sync (Q5) |
| V | Test off-device first | ✅ | ✅ | Ntp, PosixTz (DST transitions, both hemispheres), Zones (bestMatch, coverage), SyncSchedule (backoff, unsynced), quoted tokens and settings v3 are host-tested before implementation |
| VI | Deliberate, compliant, private | ✅ | ✅ | Credentials only in NVS, `***`/`••••` everywhere, never logged (incl. the setup code). The hotspot starts only on a deliberate action, uses a one-time code, auto-closes at 300 s, never overlaps a sync. NVS unencrypted: documented limitation (R4). HTTPS: not applicable in S2 (no remote HTTP API) |
| VII | Reproducible, portable | ✅ | ✅ | No new dependencies. The zone table is committed with its tzdata source version. S3 compiles |
| VIII | Simplicity | ✅ | ✅ | One network, synchronous WebServer on its own task, own 48-byte SNTP codec. No async web framework, no BLE provisioning |

**Gate: PASS.** Complexity Tracking not needed.

**SRS amendments in this branch**: FR-TIM-003 (named zones replace the interim offset); new **FR-NET-003** (setup
hotspot safeguards); S2 traceability row updated.
**SAS notes**: §6 NetService gains the setup portal; §7.1 settings v3.

## Project Structure

### Documentation (this feature)

```text
specs/003-time-sync/
├── plan.md  research.md  data-model.md  quickstart.md
├── contracts/{serial_commands.md, setup_portal.md}
├── checklists/requirements.md
└── tasks.md            # /speckit-tasks
```

### Source Code (new or changed)

```text
src/core/
├── Ntp.{h,cpp}             # SNTP request/response codec + offset (pure)
├── PosixTz.{h,cpp}         # POSIX TZ parser, offsetMinutes(utc), isDst
├── Zones.{h,cpp}           # 45 named + fixed-offset zones, bestMatch(offsetMin), find(name)
├── SyncSchedule.{h,cpp}    # due/backoff/unsynced decisions
├── CommandParser.cpp       # + double-quoted tokens
└── SettingDefs.{h,cpp}     # + Str type, wifissid/wifipsk/tzid/syncmin/lastsync, schema 3
src/services/
├── NetService.{h,cpp}      # net task, Wi-Fi lease, connect/test/scan, SNTP sync, status snapshot, `ad wifi/sync`
├── SetupPortal.{h,cpp}     # SoftAP + DNSServer + WebServer + embedded page (runs inside the net task)
├── TimeService.{h,cpp}     # zone-based offset (PosixTz), TIME UNSYNC, `ad tz`
├── SettingsStore.{h,cpp}   # string values, migration 2→3
└── Cautions.h              # + TimeUnsync
src/pages/SettingsPage.{h,cpp}   # Wi-Fi section (status, Set up Wi-Fi modal with QR, Forget, Sync now), time-zone roller
src/app/main.cpp            # NetService::begin; schedule at boot
test/
├── test_ntp/  test_posix_tz/  test_zones/  test_sync_schedule/
└── test_command_parser/, test_settings/   (extended)
docs/software_requirements_specification.md   # FR-TIM-003, FR-NET-003, traceability
docs/software_architecture_specification.md   # §6/§7.1 notes + ADR-012 row
docs/adr/012-wifi-stack-platform-service.md
```

**Structure Decision**: Continue the S0/S1 layout. The portal is a separate service file, but it executes only on the `net` task.

## Implementation Notes (for /speckit-tasks)

1. **Order**: core modules + tests (Ntp, PosixTz, Zones, SyncSchedule, quoted tokens, settings v3) → SettingsStore strings and
   migration → NetService (lease, connect, SNTP) + `ad wifi set/sync` → **checkpoint: USB provisioning and boot sync on the
   device** → TimeService zones + `ad tz` → Settings page Wi-Fi and zone UI → SetupPortal + QR modal → TIME UNSYNC → docs → validation.
2. `PfdPage` already reads `ClockSnapshot.tzOffsetMin`. TimeService now fills it from `PosixTz(zone).offsetMinutes(now)`,
   recomputed each second, so DST changeovers appear within 1 s. ClockMath stays unchanged.
3. Wi-Fi event reasons (auth fail, no AP) are captured with `WiFi.onEvent` inside NetService to classify `lastResult`.
4. The setup access code and PSK must never reach `Log`. Add a host test that `formatSettingForOutput(wifipsk)` gives `***`, and a review
   grep in polish.
5. Measure (Q5): heap before/during/after a sync, and PFD FPS with `ad att sweep on` during repeated `ad sync`. If heap <40 KB, lower
   the Wi-Fi RX buffers via `WiFi`/`esp_wifi_set_config` or restrict the setup scan list. Record this in validation.md.
6. At the end: update `docs/product_roadmap.md` per the **status tracking rule** (S2 → ✅ Done after merge).

## Complexity Tracking

Not needed. The Constitution Check passed with no violations.
