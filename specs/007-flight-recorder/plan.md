# Implementation Plan: G-Force & Flight Recorder (Slice S6)

**Branch**: `007-flight-recorder` | **Date**: 2026-11-12 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/007-flight-recorder/spec.md`

**Implements**:
- AD-SAS-001 §4 (`IBoard::stepCount`, plus motion configuration and peak g);
- §5.1 (`svc` tick hosts the Recorder) and §5.3 (eject sequence: the save-on-demand that S7 calls);
- §6 (`RecorderService`, `RecorderSnapshot`);
- §7.1 (Recorder in NVS `rec`; `RTC_DATA_ATTR` follows in S7);
- §8 (`RecorderPage`; PFD MAX G / FLT NM slots).

## Summary

The BHI260AP's **step counter** (1 Hz) and **corrected accelerometer** (50 Hz) are enabled by a new
`IBoard::configureMotion()`, called from `RecorderService::onImuReady()` once the deferred probe has started the
sensor hub (≈3.4 s after boot, next to `AttitudeService::onImuReady`). The board caches the latest step count and the running peak |a| in g; the
**RecorderService** (svc tick, 1 Hz) polls `stepCount()` and `takePeakG()`.

Pure core modules turn the readings into records:
- `StepAccumulator`: sensor restarts and implausible jumps;
- `DayLog`: today + 7 previous days; local-midnight rollover; undated, backward-clock and skipped-day rules;
- `FlightMath`: NM, centi-g, display formats;
- `RecBlob`: a versioned, CRC-checked 100-byte record.

The record is saved in its own NVS namespace `rec`:
- every 10 minutes if changed;
- at each rollover;
- on `esp_restart()` through a shutdown handler.

So Restore defaults keeps the history, and a power cut loses ≤10 minutes.

The PFD fills its MAX G / FLT NM slots, and a **Recorder page** replaces "PAGE B": today's values and a tappable
7-bar chart. The stride is setting v7 (cm). Console: `ad rec …`.

## Technical Context

**Language/Version**: C++17 (xtensa GCC 8.4, Arduino-ESP32 2.0.17 / IDF 4.4). Host tests: WinLibs GCC.

**Primary Dependencies**:
- SensorLib `SensorBHI260AP` via LilyGoLib (already pinned) — virtual sensors `STEP_COUNTER` (52) and
  `ACCEL_CORRECTED` (4);
- Arduino `Preferences` (NVS);
- LVGL 9.6;
- **no new libraries**.

**Storage**:
- NVS namespace `rec`, blob `day` (100 B);
- settings schema **v7** (`stride`).

**Testing**:
- Unity on `env:native`:
  - `test_day_log` (rollover, undated, backward clock, time zone change, skipped days, 7-day cap, weekday,
    localDay floor);
  - `test_step_acc` (deltas, sensor restart, implausible jump, wrap of millis);
  - `test_flight_math` (NM, formats, centi-g saturation);
  - `test_rec_blob` (round-trip, CRC/version/length rejects);
  - `test_settings` v7.
- On-device: quickstart Q1–Q7. Walking and flicks need the wearer.

**Target Platform**: T-Watch Ultra. The T-Watch S3 must compile (`configureMotion` returns false → "--").

**Project Type**: Embedded firmware.

**Performance Goals**:
- PFD median ≥28 fps with the recorder active;
- NM/Max G on the face within 1 s;
- rollover within 2 s of midnight;
- a save ≤20 ms on the svc task.

**Constraints**:
- motion sensor access only in `src/board/` (II);
- callbacks on the UI task do O(1) work only (IV);
- no new power rails (III);
- internal heap ≥40 KB.

**Scale/Scope**:
- 4 core modules;
- 1 service;
- IBoard +3 methods;
- 1 page (replaces the placeholder) + PFD wiring + 1 settings row;
- 6 serial commands;
- schema v7.

No NEEDS CLARIFICATION remains (research R1–R10). The availability of the step counter in the KLIO firmware is
**verified first** (R1 fallback ready).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.* (Constitution v1.0.3)

| # | Principle | Pre | Post | Evidence |
| :-- | :--- | :---: | :---: | :--- |
| I | Every slice ships | ✅ | ✅ | A missing step counter or accelerometer → "--" and the watch still works; the recorder never blocks boot; corrupt saved data → start empty |
| II | Layered architecture | ✅ | ✅ | Sensor hub only in `src/board/` behind `IBoard::configureMotion/stepCount/takePeakG`; NVS through `Preferences` in a service (as `SettingsStore`); pure core for all rules; the page reads `RecorderSnapshot` |
| III | Battery is a feature | ✅ | ✅ | No new rail; the sensor hub is already powered for attitude. **Duty cycle:** STC 1 Hz + ACC 50 Hz on the hub; NVS write ≤ every 10 min when changed. S7 measures battery life with the recorder on |
| IV | Frame time is sacred | ✅ | ✅ | UI-task callbacks are O(1); polling and saving on the svc task (core 0). FPS measured (Q7) |
| V | Test off-device first | ✅ | ✅ | DayLog, StepAccumulator, FlightMath, RecBlob, settings v7 tested first; on-device Q1–Q7 |
| VI | Deliberate, compliant, private | ✅ | ✅ | Steps and g stay on the watch (NVS); nothing leaves the device; no location |
| VII | Reproducible, portable | ✅ | ✅ | No new libraries; S3 compiles with "--" |
| VIII | Simplicity | ✅ | ✅ | Hub step counter instead of our own pedometer; 7 `lv_obj` bars instead of a chart library; one blob |

**Gate: PASS.** Complexity Tracking is not needed.

**SRS amendments in this branch**:
- FR-REC-001 note: stride 0.30–1.50 m, setting `stride`;
- FR-REC-005 note: saved every 10 min when changed, at rollover and on restart; deep sleep via S7;
  Restore defaults keeps the data.

**SAS notes** (no ADR needed: refinements of the existing design, not deviations):
- §4 `IBoard`: `configureMotion(accelHz)`, `stepCount(out)` (was `stepCount()`: the output-parameter form can
  report "unavailable"), `takePeakG(peak, last, rangeG)`;
- §6 RecorderService row;
- §7.1: NVS `rec` every 10 min when changed (stricter than the planned hourly) + rollover + shutdown handler;
- §8: RecorderPage replaces PAGE B.

## Project Structure

### Documentation (this feature)

```text
specs/007-flight-recorder/
├── plan.md  research.md  data-model.md  quickstart.md
├── contracts/serial_commands.md
├── checklists/requirements.md
└── tasks.md            # /speckit-tasks
```

### Source Code (new or changed)

```text
src/core/
├── DayLog.{h,cpp}               # today + 7 days, rollover rules, localDayOf, weekday (pure)
├── StepAccumulator.{h,cpp}      # raw count → deltas (pure)
├── FlightMath.{h,cpp}           # NM, centi-g, formats (pure)
├── RecBlob.{h,cpp}              # encode/decode + CRC-16 (pure)
└── SettingDefs.{h,cpp}          # + stride; schema 7
src/board/IBoard.h, BoardUltra.cpp, BoardS3.cpp   # configureMotion, stepCount, takePeakG
src/services/RecorderService.{h,cpp}              # svc tick, NVS `rec`, shutdown handler, snapshot, `ad rec …`
src/pages/RecorderPage.{h,cpp}                    # replaces PlaceholderPage "PAGE B"
src/pages/PfdPage.cpp                             # MAX G / FLT NM from RecorderSnapshot
src/pages/SettingsPage.{h,cpp}                    # RECORDER: Stride −/+
src/ui/Page.h                                     # PageId::PlaceholderB → PageId::Recorder
src/app/main.cpp                                  # RecorderService::begin + tick; onImuReady from deferredProbeTask; RecorderPage
test/test_day_log/ test_step_acc/ test_flight_math/ test_rec_blob/  (+ test_settings extended)
docs/software_requirements_specification.md, docs/software_architecture_specification.md, README.md
```

**Structure Decision**: Continue the S0–S5 layout.

## Implementation Notes (for /speckit-tasks)

1. **Order:**
   1. **Probe the motion sources first:** `configureMotion` + a temporary log → STC present? counts while walking?
      0 after reboot? ACC scale at rest ≈1 g? range accepted? If STC is absent, take the R1 fallback.
   2. Core tests → core.
   3. Settings v7.
   4. IBoard motion.
   5. RecorderService + `ad rec`.
   6. PFD slots.
   7. RecorderPage.
   8. Settings row.
   9. Docs and validation.
2. **`localDay` and minute of day** come from `ClockSnapshot` (`utc + tzOffsetMin`) — the same source as the PFD and
   the S5 chime.
3. **The shutdown handler** must not log through the serial task (it runs inside `esp_restart`); write NVS directly
   and keep it short. If a save in the handler proves unreliable, call `saveNow()` explicitly from `cmdReboot` and
   note it.
4. **Chart scale:** the largest of the 7 bars = full height; all zero → flat outlines.
5. **Measure:** time per save (ms), FPS with recorder on, the at-rest g and the walking NM error.
6. **At the end:** update `docs/product_roadmap.md` per the status tracking rule (S6 → ✅; **M2 → done** when S6
   merges, if its definition of done is met).

## Complexity Tracking

Not needed. The Constitution Check passed with no violations.
