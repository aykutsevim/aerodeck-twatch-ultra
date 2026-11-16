# Implementation Plan: Drop-Tank Chronograph (Slice S8)

**Branch**: `009-drop-tank-chrono` | **Date**: 2026-11-13 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/009-drop-tank-chrono/spec.md`

**Implements**:
- AD-SAS-001 §6: `ChronoService`, `ChronoSnapshot`, the `ChronoExpired` → `AlertManager` path.
- §5.3: the eject pre-sleep hook and the timer wake.
- §7.1: NVS `chrono`, settings v9.
- §8: `ChronoPage`, the tank widget.
- SRS FR-CHR-001…005.

## Summary

- **Model:** a pure `core::ChronoModel` runs the countdown and Pomodoro on **absolute UTC end times** (R1). It does
  start, pause, resume, reset, expiry and work/break transitions, and restore after a restart.
- **Service:** `ChronoService` on the svc task (50 ms tick) fires within one tick of the target.
  - It raises **BINGO FUEL** (new caution, vibration and the S5 "Bingo Fuel" clip) and **BREAK OVER** (new caution,
    chime).
  - It persists the state in NVS `chrono` (on changes only) and publishes `ChronoSnapshot`.
  - It registers S7's **pre-sleep hook**: the wake time is the next event − 5 s (boot lead time), or it postpones
    the eject when ≤ 6 s remain (R3).
- **Board:** the timer wake uses the **PCF85063 RTC alarm** on GPIO1 (crystal accuracy) instead of the drifting ESP
  RC timer (R2). This is an internal change to `BoardUltra::deepSleep`.
  - GPIO1's RTC pull-up is on during sleep (open-drain INT).
  - The alarm is relative to the RTC's reading, so it also works when the clock was never set.
- **Boot clock:** `TimeService` sets the system clock to the RTC seconds + 0.5 s, so an alert after a wake lands
  within ±0.55 s (R1).
- **Page:** a **CHRONO** page between RECORDER and SETTINGS shows:
  - a drop-tank gauge that drains;
  - a DSEG7 MM:SS readout;
  - the phase and cycle count;
  - presets 5/15/25/50 and custom −/+;
  - Countdown/Pomodoro;
  - Start/Pause and Reset.
- **Console:** `ad chrono …`.

## Technical Context

**Language/Version**: C++17 (xtensa GCC 8.4, Arduino-ESP32 2.0.17 / IDF 4.4). Host tests: WinLibs GCC.

**Primary Dependencies**:
- SensorLib `SensorPCF85063` (alarm; already pinned, used through `src/board/` only);
- `Preferences` (NVS);
- LVGL 9.6;
- **no new libraries**.

**Storage**:
- NVS namespace `chrono`, blob `st` (versioned + CRC-16), written only on state changes;
- settings schema **v9**: `chrlen`, `chrmode`, `chrwork`, `chrbrk`.

**Testing**:
- Unity on `env:native`:
  - `test_chrono_model`:
    - start, pause, resume, reset;
    - expiry exactly at the target;
    - fraction;
    - Pomodoro work → break → work, with cycle counts;
    - `nextEventUtcMs`;
    - restore after restart (running, paused, expired-late, missed Pomodoro transitions);
    - clamping after a clock reset;
    - a clock correction of Δ;
  - `test_chrono_blob`: round trip, rejects;
  - `test_settings`: v9;
  - `test_alerts`: the two new ids.
- On-device quickstart Q1–Q7. Expiry timing is checked by script against the PC clock.

**Target Platform**: T-Watch Ultra. The T-Watch S3 must compile: its `deepSleep` returns false, so the timer
simply keeps running awake there.

**Project Type**: Embedded firmware.

**Performance Goals**:
- alert within ±1 s of the target in all four cases (page, other page, screen off, ejected);
- ≤ 1 NVS write per state change;
- PFD ≥ 30 fps median while a timer runs.

**Constraints**:
- RTC and alarm access only in `src/board/` (II);
- no LVGL in the service;
- the page reads the snapshot;
- the idle timer adds no wakeups beyond the existing svc tick;
- the eject budget stays < 2 s.

**Scale/Scope**:
- 2 pure core modules (`ChronoModel`, `ChronoBlob`) + 2 alert ids;
- 1 service;
- 1 page + 1 widget (`TankWidget`);
- `BoardUltra::deepSleep` uses the RTC alarm;
- `PowerService` gets the postpone sentinel;
- settings v9;
- about 6 serial commands.

No NEEDS CLARIFICATION remains (research R1–R7).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.* (Constitution v1.0.4)

| # | Principle | Pre | Post | Evidence |
| :-- | :--- | :---: | :---: | :--- |
| I | Every slice ships | ✅ | ✅ | Timer failures never block boot. A corrupt blob gives an idle timer. Without the RTC, the ESP timer fallback is used. The S3 keeps timing awake |
| II | Layered architecture | ✅ | ✅ | PCF85063 alarm only in `BoardUltra`, behind the existing `deepSleep(mask, timerSec)`. Pure `ChronoModel`. The service uses `AlertManager` and `PowerService` APIs. The page reads `ChronoSnapshot` and posts commands |
| III | Battery is a feature | ✅ | ✅ | No rail is used. The idle timer costs nothing; running, it is a 50 ms tick check on the existing svc task. Ejected, it costs one RTC alarm with no extra current, and one boot at the target. **Duty:** NVS writes only on state changes |
| IV | Frame time is sacred | ✅ | ✅ | Logic on the svc task. Tank redraw ≤ 1 Hz, only while CHRONO is visible. PFD fps measured while running (Q7) |
| V | Test off-device first | ✅ | ✅ | `ChronoModel`, `ChronoBlob`, settings v9 and alert ids are tested first. Q1–Q7 on the device |
| VI | Deliberate, compliant, private | ✅ | ✅ | Reset is a single tap but harmless (not destructive or transmitting). Nothing leaves the watch |
| VII | Reproducible, portable | ✅ | ✅ | No new libraries. The S3 compiles |
| VIII | Simplicity | ✅ | ✅ | No long-break logic, one timer, absolute time instead of tick counting, the existing S7 hook |

**Gate: PASS.** Complexity Tracking is not needed.

**SRS amendments**:
- FR-CHR-001 note: custom time 1–120 min.
- FR-CHR-004 note: RTC-alarm wake, boot lead time, eject postponed in the last 6 s.
- FR-CHR-005 note: 25/5 default, break 5/10/15, BREAK OVER chime.

**SAS notes**:
- §5.3: the timer wake is the RTC alarm; the hook can postpone the eject.
- §6: `ChronoService` row.
- §7.1: NVS `chrono` + settings v9.
- §8: the CHRONO page in the ring.
- ADR-014 addendum: the timer wake source is the RTC alarm (GPIO1).

## Project Structure

### Documentation (this feature)

```text
specs/009-drop-tank-chrono/
├── plan.md  research.md  data-model.md  quickstart.md
├── contracts/serial_commands.md
├── checklists/requirements.md
└── tasks.md            # /speckit-tasks
```

### Source Code (new or changed)

```text
src/core/ChronoModel.{h,cpp}        # state machine on absolute UTC ms (pure)
src/core/ChronoBlob.{h,cpp}         # versioned, CRC-checked state record (pure)
src/core/Alerts.{h,cpp}             # + BingoFuel (42), BreakOver (43)
src/core/SettingDefs.{h,cpp}        # + chrlen, chrmode, chrwork, chrbrk; schema 9
src/services/TimeService.cpp        # boot: system clock = RTC seconds + 0.5 s (R1)
src/board/BoardUltra.cpp            # deepSleep timer → PCF85063 alarm on GPIO1; wakeCause(GPIO1)=Timer; clear alarm at boot
src/services/ChronoService.{h,cpp}  # svc tick, NVS `chrono`, alerts, snapshot, pre-sleep hook, `ad chrono …`
src/services/PowerService.{h,cpp}   # kPostponeSleep sentinel → eject aborted
src/ui/Page.h                       # PageId::Chrono
src/ui/widgets/TankWidget.{h,cpp}   # drop-tank gauge (custom draw, fill fraction)
src/pages/ChronoPage.{h,cpp}        # page
src/app/main.cpp                    # ChronoService::begin/tick; register ChronoPage
test/test_chrono_model/ test_chrono_blob/  (+ test_settings v9, test_alerts)
docs/software_requirements_specification.md, docs/software_architecture_specification.md, docs/adr/014-…, README.md
```

**Structure Decision**: Continue the S0–S7 layout.

## Implementation Notes (for /speckit-tasks)

1. **Order:**
   1. **RTC-alarm wake probe first:** extend `ad probe s7 sleep` (or add `ad probe s8 alarm <s>`). Check that
       sleeping with an RTC alarm at +60 s wakes at 60 ± 1 s (PC clock), and that `wakeCause` = Timer.
   2. Core tests → core.
   3. Alert ids.
   4. Settings v9.
   5. `ChronoService` + console.
   6. Pre-sleep hook + postpone.
   7. Tank widget + page.
   8. Measurements.
   9. Docs.
2. **The svc tick is 50 ms:** `ChronoService::tick` compares now ≥ nextEvent each tick (cheap), so expiry latency is
   ≤ 50 ms awake.
3. **Wake boot:** `ChronoService::begin` (early in `setup`, after `TimeService::begin`) restores and schedules. If
   the target is still ahead (the normal case, about 1.5 s after a lead-time wake), it fires on time from the tick.
4. **Measurements:**
   - expiry error vs. the PC clock: script `ad chrono start 60`, then read the alert log timestamp against the
     target;
   - eject case: `ad wake` + `ad chrono` after the wake;
   - PFD fps while running.
5. **At the end:** update `docs/product_roadmap.md` (S8 → ✅, PR number; **M3 → done** if the definition of done
   holds).

## Complexity Tracking

Not needed. The Constitution Check passed with no violations.
