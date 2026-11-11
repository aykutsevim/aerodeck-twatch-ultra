# Implementation Plan: PFD Watch Face (Slice S1)

**Branch**: `002-pfd-watch-face` | **Date**: 2026-11-11 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/002-pfd-watch-face/spec.md`

**Implements**: AD-SAS-001 §5.2 (snapshot contract), §6 (AttitudeService, TimeService-lite, PowerService),
§8 (UI architecture: horizon, tapes, fonts). Deviates from §5.1 (no separate attitude task) and refines §8 (custom-draw
horizon instead of transforms). See ADR-010/011.

## Summary

Replace the S0 placeholder Home with a Primary Flight Display:
- a custom-drawn artificial horizon fed by the BHI260AP's on-chip gravity fusion (100 Hz), with a hysteretic
  LOCK indicator;
- hour, minute and seconds-heading tapes, plus ZULU/LOCAL corners, from a UTC RTC and a configurable fixed offset;
- an EICAS strip (battery %, estimated drain rate %/h, PMU temperature, charging annunciator);
- a LOW FUEL caution;
- placeholder MAX G / FLIGHT NM.

The main technical risk is ≥30 FPS at 410×502. It is handled by redrawing only the ADI and heading strip each
frame, with a performance spike gating the rest of the work (research R2).

## Technical Context

**Language/Version**: C++17 (xtensa GCC 8.4 via Arduino-ESP32 2.0.17). Host tests use WinLibs GCC 16.2.

**Primary Dependencies**: S0 stack (LilyGoLib 0.2.0, LVGL 9.6.0, SensorLib 0.3.4 for BHI260AP + PCF85063A,
XPowersLib 0.3.3). New: DSEG7 Classic (SIL OFL) converted with `lv_font_conv` (via `npx`; generated files committed).

**Storage**: NVS settings schema v2 (`tzoff`). RTC (PCF85063A) holds UTC.

**Testing**: Unity on `env:native` for the new core modules (Attitude, LockDetector, ClockMath, FuelFlow, FuelState,
SettingDefs step). On-device checks per quickstart P2–P7 via the serial console.

**Target Platform**: T-Watch Ultra (primary). T-Watch S3 must compile (`configureAttitude` returns false → ATT FAIL).

**Project Type**: Embedded firmware (single PlatformIO project).

**Performance Goals**: Horizon ≥30 FPS sustained while moving (SC-001); motion→display ≤50 ms (SC-002);
heading tape scrolls smoothly; EICAS ≤1 s stale.

**Constraints**: LVGL only on the UI task; I2C lock for BHI FIFO and RTC; heap ≥40 KB; no new vendor patch unless the
R2 spike fails all cheaper options.

**Scale/Scope**: 1 page replaced, 3 widgets, 2 small services, ~6 core modules, 1 setting, 3 serial commands.

No NEEDS CLARIFICATION remains (research R1–R10). One item is **verified on device by design**: the BHI axis
mapping (spike).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | Principle | Pre | Post | Evidence |
| :-- | :--- | :---: | :---: | :--- |
| I | Every slice ships | ✅ | ✅ | PFD degrades: ATT ALIGN/FAIL flag, "--" for an invalid clock or PMU, S0 cautions intact. The S3 build still compiles |
| II | Layered architecture | ✅ | ✅ | BHI/RTC access only through new `IBoard` methods. Math in `src/core/`. Widgets only in `src/ui/`. The BHI callback runs on the UI task (R1) and writes a Snapshot. No LVGL in services |
| III | Battery is a feature | ✅ | ✅ | No new rails. The frame timer runs only while the PFD is visible (R10). Invalidation only on change. GRA at 100 Hz (S7 can lower it). Duty: N/A (no radio) |
| IV | Frame time is sacred | ✅ | ✅ | ≥30 FPS gate via spike before the rest of the UI work. RTC/settings I/O stays off the UI path, except the BHI FIFO read, which LilyGoLib does on the UI task (small, IRQ-driven; measured via `ad att` latency) |
| V | Test off-device first | ✅ | ✅ | All new math is host-tested (attitude, lock hysteresis, clock split/rollover/ISO parse, fuel regression, low-fuel hysteresis, step validation) |
| VI | Deliberate, compliant, private | ✅ | ✅ | `ad time set` is a diagnostic command. No RF, no secrets |
| VII | Reproducible, portable | ✅ | ✅ | Generated font `.c` files are committed, with the OFL licence and generation command. No new libraries. Layout via `Theme::S()` |
| VIII | Simplicity | ✅ | ✅ | Fixed UTC offset instead of TZ rules (S2). FUEL FLOW from the existing % data. No new tasks |

**Gate: PASS.** No Complexity Tracking needed.

**ADRs added in this branch**:
- **ADR-010**: Attitude from the BHI260AP gravity virtual sensor, delivered through LilyGoLib's event loop on the UI task.
  There is no dedicated attitude task (SAS §5.1 amended).
- **ADR-011**: Horizon drawn with a custom `DRAW_MAIN` (triangles and lines) and partial invalidation, instead of
  object/layer transforms (SAS §8 refined). Includes the measured FPS from the spike.

**Spec/SRS amendments in this branch**: SRS FR-PFD-007 "current draw (mA)" → "drain rate (%/h)" (S0 finding:
the AXP2101 has no battery-current ADC). SRS FR-PFD-006/§3.3 note the fixed offset until S2.

## Project Structure

### Documentation (this feature)

```text
specs/002-pfd-watch-face/
├── plan.md  research.md  data-model.md  quickstart.md
├── contracts/{serial_commands.md, internal_interfaces.md}
├── checklists/requirements.md
└── tasks.md            # /speckit-tasks
```

### Source Code (repository root; new or changed)

```text
src/core/
├── Attitude.{h,cpp}        # gravity → pitch/roll, EMA, vertical guard
├── LockDetector.{h,cpp}    # ±3°/0.5 s in, ±5° out
├── ClockMath.{h,cpp}       # splitLocal, ISO-8601 parse, plausibility
├── Fuel.{h,cpp}            # FuelFlow regression + FuelState hysteresis
├── PfdLayout.{h,cpp}       # pure layout boxes for any w×h; host-tested for no overlaps (FR-018)
└── SettingDefs.{h,cpp}     # + step field, tzoff, schema 2
src/board/
├── IBoard.h                # + configureAttitude, readRtc, writeRtc
├── BoardUltra.cpp          # BHI GRA config/callback; PCF85063 read/write
└── BoardS3.cpp             # RTC via PCF8563; configureAttitude → false
src/services/
├── AttitudeService.{h,cpp} # Snapshot<AttitudeSnapshot>, status, latency stats, `ad att`
├── TimeService.{h,cpp}     # RTC → system time, validity, CLOCK NOT SET, `ad time`
├── PowerService.{h,cpp}    # + FuelFlow/FuelState, LOW FUEL caution, `ad fuel`
└── SettingsStore.cpp       # migration 1→2
src/ui/widgets/
├── AdiWidget.{h,cpp}       # custom DRAW_MAIN horizon, ladder, aircraft symbol, LOCK, flags
├── TapeWidget.{h,cpp}      # vertical/horizontal tape with value box
└── EicasWidget.{h,cpp}
src/ui/fonts/
├── dseg7_40.c  dseg7_28.c  OFL.txt
src/pages/
├── PfdPage.{h,cpp}         # replaces HomePage (removed)
└── SettingsPage.cpp        # + Local offset row
test/
├── test_attitude/  test_lock/  test_clock_math/  test_fuel/  test_pfd_layout/   (+ test_settings updated)
tools/gen_fonts.md
docs/adr/010-attitude-from-bhi-gravity.md
docs/adr/011-custom-draw-horizon.md
docs/software_requirements_specification.md   # FR-PFD-007 amended
docs/software_architecture_specification.md   # §5.1/§8 notes + ADR rows
```

**Structure Decision**: Continue the S0 layout. Widgets get their own `src/ui/widgets/` folder for reuse (S6 recorder and
S9 PPI will add more).

## Implementation Notes (for /speckit-tasks)

1. **Order**: core + tests → IBoard additions (RTC, GRA) → **spike T-ATT** (axis mapping, `ad att`) → **spike
   T-PERF** (bare AdiWidget on the Home page, measure FPS; gate ≥30) → TimeService/tapes → EICAS/fuel → fonts →
   full PfdPage layout → settings tzoff → docs/ADRs → validation.
2. The BHI GRA must be configured **after** the deferred probe marks the IMU Ok. The probe task calls
   `AttitudeService::onImuReady(ok)`, which calls `board().configureAttitude()` (I2C lock inside).
3. The GRA callback runs inside `sensor.update()` on the UI task while `BoardUltra::loop()` holds the I2C lock. It must
   only do the math and write the Snapshot (no LVGL, no logging at INFO).
4. The ADI invalidates itself from the frame timer, by comparing the snapshot `seq` and the delta ≥0.2°.
5. The heading tape uses `gettimeofday()` ms each frame. Hour/minute recompute only when the value changes.
6. `HomePage` is removed. `PageId::Home` is now `PfdPage`.
7. If spike T-PERF misses 30 FPS after options (a)–(c) in R2, stop and ask before applying (d) (vendor patch).

## Complexity Tracking

Not needed. The Constitution Check passed with no violations.
