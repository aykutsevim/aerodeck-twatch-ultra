# Implementation Plan: Platform Foundation (Slice S0)

**Branch**: `001-platform-foundation` | **Date**: 2026-11-11 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-platform-foundation/spec.md`

**Implements**: AD-SAS-001 §3 (layers), §4 (Board HAL), §5.1–5.2 (tasks, UI contract), §6 (PageManager,
Theme, SettingsStore, PowerService, Log), §7.1 (storage, amended), §9 (build), §10 (verification)

## Summary

Replace the hello-world app with the AeroDeck skeleton:
- a board abstraction (`IBoard` + `BoardUltra`/`BoardS3`) that never aborts on optional-peripheral
  failure;
- a wrapping page ring with button-to-home navigation and a page indicator;
- NVS-backed, schema-versioned settings with a Settings page;
- reference-counted power-rail leases;
- visual cautions;
- levelled serial logging plus a serial command console;
- an FPS/heap overlay;
- safe-mode protection against boot loops;
- reproducible builds: the LilyGoLib IDF-4.4 fix is applied automatically by a `post:` extra
  script (verified to run after dependency install and before compilation), plus an
  `env:native` Unity test environment.

## Technical Context

**Language/Version**: C++17 (GCC 8.4 xtensa toolchain from Arduino-ESP32 2.0.17). Host tests use
MinGW-w64 GCC.

**Primary Dependencies** (all exact pins, see T005): Arduino-ESP32 2.0.17 / ESP-IDF 4.4
(`espressif32@6.12.0`), LilyGoLib 0.2.0, LVGL 9.6.0, RadioLib 7.8.1, XPowersLib 0.3.3,
SensorLib 0.3.4, FreeRTOS (bundled), Preferences (NVS).

**Storage**: NVS namespace `aerodeck` (settings). `RTC_NOINIT` boot record. FAT `ffat` partition
reserved for later assets (ADR-009).

**Testing**: PlatformIO Unity on `env:native` for `src/core/`. On-device acceptance per quickstart.md.

**Target Platform**: LilyGO T-Watch Ultra (ESP32-S3, 16 MB flash, 8 MB PSRAM, 502×410 AMOLED).
T-Watch S3 compile-only.

**Project Type**: Embedded firmware (single PlatformIO project).

**Performance Goals**: Boot-to-home ≤3 s; ≥30 FPS during transitions; transitions ≤300 ms;
button-to-home ≤0.5 s.

**Constraints**: Internal free heap ≥40 KB (≈137 KB taken by DMA draw buffers, see research R7);
no UI stall >33 ms; LVGL only on the UI task (`LV_OS_NONE`); optional rails off after boot.

**Scale/Scope**: 4 pages, 5 settings, ~13 peripherals tracked, ~25 source files.

No NEEDS CLARIFICATION remains. All unknowns are resolved in [research.md](./research.md) (R1–R11).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | Principle | Pre-design | Post-design | Evidence |
| :-- | :--- | :---: | :---: | :--- |
| I | Every slice ships | ✅ | ✅ | S0 ends with flashable firmware. Optional-peripheral failures → cautions (R2). PMU pre-probe, safe mode and display-stage headless fallback stop boot loops (R3) |
| II | Layered architecture | ✅ | ✅ | `src/{board,core,services,ui,pages,app}`. Vendor types only in `src/board/`. LVGL only in `loop()` (R8). Touch read wrapped with the I2C lock |
| III | Battery is a feature | ✅ | ✅ | `RailLease` refcount. All optional rails forced off after boot (data-model §5). V7 verifies |
| IV | Frame time is sacred | ✅ | ✅ | PMU polling, NVS writes and the serial console on the `svc` task (core 0). FPS/heap overlay ships in S0. Heap budget measured (R7) |
| V | Test off-device first | ✅ | ✅ | `env:native` + Unity for `src/core/` (settings validation, schema migration decision, log formatting, nav state machine, command parser). On-device checks in quickstart |
| VI | Deliberate, compliant, private | ✅ | ✅ | Restore-defaults needs confirmation. Secret flag masks values in UI/log/serial. No RF transmit in S0 |
| VII | Reproducible, portable builds | ✅ | ✅ | `post:` patch script (R1). Project-owned partition CSV (R6). **All `lib_deps` pinned to exact versions and Git URLs to commit SHAs, and `native` platform pinned (T005). The upstream `platformio.ini` used `^` ranges and unpinned Git URLs, which is non-compliant and fixed in this slice.** `twatchs3` must compile. Layout uses Theme `S(px)` |
| VIII | Simplicity | ✅ | ✅ | Minimal caution banner instead of the full AlertManager (S4). No speculative abstractions beyond the SAS interfaces |

**Gate result: PASS.** No violations; Complexity Tracking not needed.

**SAS deviations (ADRs added in this branch)**:
- **ADR-008**: LilyGoLib patch via `post:` script, not `pre:` (SAS §9). A `pre:` script runs before
  `lib_deps` install on a clean checkout (verified).
- **ADR-009**: FAT (`ffat`, mounted and USB-exposed by LilyGoLib) instead of LittleFS for assets
  (SAS §7.1).

## Project Structure

### Documentation (this feature)

```text
specs/001-platform-foundation/
├── plan.md              # This file
├── research.md          # Phase 0 (R1–R11)
├── data-model.md        # Phase 1
├── quickstart.md        # Phase 1 validation guide
├── contracts/
│   ├── serial_commands.md
│   └── internal_interfaces.md
├── checklists/requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks — not created here)
```

### Source Code (repository root)

```text
platformio.ini                 # envs: twatch_ultra (default), twatchs3, native; extra_scripts pre:tools/fw_version.py, post:tools/patch_lilygolib.py;
                               # board_build.partitions = partitions/aerodeck_16MB.csv; emulator + tlora_pager envs removed
partitions/aerodeck_16MB.csv
tools/patch_lilygolib.py
tools/fw_version.py
src/
├── app/
│   ├── main.cpp               # setup(): boot record → settings → board().begin → services → pages; loop(): UI pump
│   └── version.h              # FW version (from build flag / git describe)
├── board/                     # ONLY place with LilyGoLib includes and board #ifdefs
│   ├── IBoard.h  I2cLock.h    # IBoard includes core/Types.h (never the reverse)
│   ├── BoardUltra.cpp
│   └── BoardS3.cpp
├── core/                      # pure C++, host-tested; includes nothing outside core/ and the std lib
│   ├── Types.h                # Cap, Rail, Periph, PeriphState, DisplayInfo, PmuReading, BootResult
│   ├── SimFail.{h,cpp}        # simfail bit ↔ Periph ↔ name
│   ├── SettingDefs.{h,cpp}    # registry, validation, migration decision
│   ├── NavModel.{h,cpp}       # ring/next/prev/button/lastNonHome/pending logic
│   ├── CommandParser.{h,cpp}  # `ad …` tokenizer + dispatch table
│   ├── LogFormat.{h,cpp}
│   └── BootGuard.{h,cpp}      # fast-fail counter + stage breadcrumb → Normal/Safe/Headless
├── services/
│   ├── EventBus.{h,cpp}  Snapshot.h
│   ├── SettingsStore.{h,cpp}  # Preferences adapter over core/SettingDefs
│   ├── PowerService.{h,cpp}   # RailLease, PMU poll 1 Hz
│   ├── Cautions.{h,cpp}
│   ├── Log.{h,cpp}
│   ├── SerialConsole.{h,cpp}  # on svc task
│   └── SvcTask.{h,cpp}        # core-0 cooperative tick
├── ui/
│   ├── Page.h  PageManager.{h,cpp}
│   ├── Theme.{h,cpp}          # S(px), palette tokens, styles
│   ├── CautionBanner.{h,cpp}  PageIndicator.{h,cpp}  DebugOverlay.{h,cpp}
└── pages/
    ├── HomePage.{h,cpp}       # placeholder: title + clock-from-millis
    ├── PlaceholderPage.{h,cpp}
    └── SettingsPage.{h,cpp}   # brightness, overlay, log level, restore defaults, About/Status
test/
├── test_settings/  test_nav/  test_command_parser/  test_boot_guard/  test_log_format/
docs/
├── adr/008-lilygolib-patch-post-script.md
├── adr/009-fat-instead-of-littlefs.md
└── software_architecture_specification.md   # §7.1, §9 and ADR table amended
```

**Structure Decision**: A single PlatformIO project using the AD-SAS-001 §9 layout. `src/core/`
is compiled alone by `env:native` (`build_src_filter = +<core/>`) so host tests never pull in
Arduino or LVGL.

## Implementation Notes (for /speckit-tasks)

1. **Order**: build infra (patch script, partitions, envs) → core + tests → board HAL → services
   → UI framework → pages → main wiring → on-device validation.
2. `BoardUltra::begin`: I2C probe 0x34 → (absent: fatal idle loop) → compute `NO_HW_*` from
   `simfail` and safe mode → `instance.begin(flags)` → map `devices_probe` bits to `PeriphState`
   → `beginLvglHelper(instance)` → wrap the touch indev read with `I2cLock` → register the
   POWER_EVENT callback (KEY_CLICKED → `onButtonClick`) → power off optional rails.
3. Brightness steps map to `instance.setBrightness()` over 0–255 (`DEVICE_MAX_BRIGHTNESS_LEVEL`).
4. Arduino `loop()`: `board().loop()` → drain UI event queue → `PageManager` ticks →
   `lv_timer_handler()` → `delay(1)`.
5. Watchdog: subscribe the `svc` task and the loop task to the ESP-IDF task WDT (5 s).
6. `BootGuard`: count is cleared by a one-shot timer at 10 s uptime.
7. Firmware version: `-D AD_FW_VERSION=\"...\"` from `git describe --always --dirty`, injected by a
   separate `pre:tools/fw_version.py`. Build flags must be set before the build scripts clone
   their environments. Unlike the patch, this doesn't depend on `lib_deps`.

## Complexity Tracking

Not needed. The Constitution Check passed with no violations.
