# Implementation Plan: NVG Mode, Eject & Power UX (Slice S7)

**Branch**: `008-nvg-eject-power` | **Date**: 2026-11-12 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/008-nvg-eject-power/spec.md`

**Implements**:
- AD-SAS-001 §4: `IBoard` gets display on/off, deep sleep, wake cause and the NVG flush filter.
- §5.3: the eject sequence.
- §6: `PowerService` gets eject, pre-sleep hooks and profiling; a new `ScreenService` lives on the UI side.
- §7.1: settings v8.
- §8: NVG, the eject handle, the annunciator offset.
- Resolves SRS OI-1 (ADR-014).

## Summary

**Screen timeout:**
- A pure `core::ScreenPolicy` turns the AMOLED off after the idle time. The idle time is a setting `scrto`
  (10 s / **15 s** / 30 s / 1 min / 5 min / Never).
- Screen off means: display sleep-in, and the UI task skips `lv_timer_handler()`, so there is no rendering and
  touches are ignored. Services keep running.
- The screen turns back on for a wrist raise, the button (that press is consumed) or a new caution or warning.
- Wrist raises are detected by a pure `core::RaiseDetector` on the gravity samples the hub already streams.

**Pull-to-Eject:**
- A hazard-striped **EjectHandle** fills the top band of the watch face. While an alert is active, the annunciator
  moves down one band on the watch face only (research R8).
- The pure `core::EjectGesture` requires a double tap (≤400 ms), then a pull of ≥40 % of the travel within 3 s.
- `PowerService::eject()` runs on the svc task: recorder `saveNow`, stop audio, haptics and network, pre-sleep hooks
  (for S8), quiesce the UI, then `IBoard::deepSleep()`.
- `deepSleep()` is AeroDeck's own sequence built from LilyGoLib's public calls, without the vendor's 2 s delay, and
  with timer wake available for S8 (research R6).
- The button wakes the watch through the PMU IRQ (ext1). Wake from wrist movement is a time-boxed spike behind
  `AERODECK_IMU_WAKE` (R7).

**NVG:**
- A board-level **flush filter** recolours every pixel to phosphor-green luminance, using the pure
  `core::NvgColor`.
- Brightness drops to 10 %.
- The annunciator uses the pure `core::NvgStyle`: WARNING inverse and blinking at 2 Hz, CAUTION inverse,
  ADVISORY outline.
- The whole interface turns green within one frame, with no per-page code (R1–R3).

**Power:**
- `ad pwr log on` prints a 1 Hz profile line (the AXP2101 has no current ADC, so it logs %, mV and the drain rate,
  R9).
- `docs/power_report.md` records the measured state currents and the battery-life estimate against the ≤50 mA / ≥22 h target (NFR-PWR-002 as amended in S7).

## Technical Context

**Language/Version**: C++17 (xtensa GCC 8.4, Arduino-ESP32 2.0.17 / IDF 4.4). Host tests: WinLibs GCC.

**Primary Dependencies**:
- LilyGoLib 0.2.0: `sleepDisplay`/`wakeupDisplay`, `setBrightness`, PMU/expander power control, touch sleep;
- ESP-IDF `esp_sleep_*`;
- LVGL 9 (display flush callback wrap, `lv_display_get_inactive_time`);
- **no new libraries**.

**Storage**: settings schema **v8** (`nvg`, `scrto`, `raise`) in NVS `aerodeck`. The recorder is unchanged (NVS
`rec`, saved at eject).

**Testing**:
- Unity on `env:native`:
  - `test_screen_policy`: timeout steps, Never, button consumed, raise, alert wake, setup screen keeps it on,
    input restarts the timer;
  - `test_raise_detector`: raise sequences, arm swing, already in the viewing pose, hold time, thresholds;
  - `test_eject_gesture`: full gesture fires, single tap, slow double tap, pull too short, pull after 3 s,
    horizontal move, release early, re-arm;
  - `test_nvg`: luminance → green mapping (black, white, mid grey, saturated red, green, blue; monotonic);
    `NvgStyle` per level and phase;
  - `test_settings`: v8.
- On-device: quickstart Q1–Q8. Raises, gestures and NVG readability need the wearer; the battery measurements need
  hours off USB.

**Target Platform**: T-Watch Ultra. The T-Watch S3 must compile:
- `deepSleep` uses the same ESP-IDF calls with the S3 pins, or returns false, and eject then logs "unsupported";
- display on/off goes through LilyGoLib;
- the flush filter is shared code in the board layer.

**Project Type**: Embedded firmware.

**Performance Goals**:
- screen off within 1 s after the timeout, on within 1 s after a raise or the button;
- eject: deep sleep within 2 s of the end of the gesture (target under 1 s);
- wake to watch face within 4 s;
- PFD median **≥30 fps** in day mode and in NVG (constitution IV), and no more than 2 fps below the T002 baseline
  (the S2–S6 no-regression check).

**Constraints**:
- LilyGoLib, ESP-IDF sleep and LVGL display internals only in `src/board/` (II);
- the flush filter is O(pixels) with small tables (IV, heap ≥40 KB);
- I2C lock for every PMU or expander access;
- rails are still switched only by `PowerService`/board deep sleep (III);
- eject is a deliberate double action (VI).

**Scale/Scope**:
- 5 pure core modules (`ScreenPolicy`, `RaiseDetector`, `EjectGesture`, `NvgColor`, `NvgStyle`);
- `IBoard` +5 methods;
- `PowerService` +eject, hooks and profiling;
- 1 UI service (`ScreenService`);
- 1 widget (`EjectHandle`);
- `AnnunciatorBar` gets NVG styling and an offset;
- Settings page: 3 rows;
- 7 serial commands;
- schema v8;
- ADR-014;
- `docs/power_report.md`.

No NEEDS CLARIFICATION remains (research R1–R10). Wrist-motion wake is resolved by the R7 spike with a defined
fallback.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.* (Constitution v1.0.3)

| # | Principle | Pre | Post | Evidence |
| :-- | :--- | :---: | :---: | :--- |
| I | Every slice ships | ✅ | ✅ | Every wake is a normal boot to the PFD (≤4 s). Without the motion sensor, raise-to-wake is unavailable and the button still works. If deep sleep is unsupported (S3 fallback), eject logs it and stays awake. A failed recorder save at eject still sleeps. "Never" keeps the old always-on behaviour |
| II | Layered architecture | ✅ | ✅ | `esp_sleep_*`, `sleepDisplay`, the PMU and the LVGL flush internals are only in `src/board/` behind `IBoard::setDisplayOn/deepSleep/wakeCause/setNvgFilter`. All rules are in pure core. `ScreenService` and `EjectHandle` run on the UI task. Eject is orchestrated by `PowerService` on the svc task; UI ↔ svc talk through EventBus and a quiesce semaphore. `ScreenService` (UI layer) calls `IBoard::setDisplayOn/setBrightnessPct` directly, as `main.cpp`'s `applySetting` already does: calling a lower layer is allowed, and the display is not a leased rail |
| III | Battery is a feature | ✅ | ✅ | This is the slice's purpose: screen off by default after 15 s; a real deep sleep with only the needed supplies kept; the RTC backup kept for the clock. **Duty cycle:** no radio or amplifier is added. Gravity streaming stays at 100 Hz (already on). Profiling logging is opt-in. NVS write: one recorder save per eject. Battery claims come only from the measured report (R9). **Rails at eject:** `IBoard::deepSleep()` switches the remaining supplies off itself, but it is invoked **only** by `PowerService::eject()`, so rail control stays inside PowerService's sequence; the lease refcounts become void because the chip resets on wake (recorded in ADR-014) |
| IV | Frame time is sacred | ✅ | ✅ | The flush filter costs about 6 ops per pixel on the UI task; FPS is measured in day mode and NVG against the constitution's ≥30 fps (Q6, SC-007), with a fallback in R1. Screen off stops rendering, so it frees CPU. The eject work runs on core 0 |
| V | Test off-device first | ✅ | ✅ | All 5 core modules and settings v8 are tested first. On-device Q1–Q8 cover SC-001…SC-007 |
| VI | Deliberate, compliant, private | ✅ | ✅ | Eject needs a double tap followed by a pull (NFR-USE-003) and is unit-tested against 7 wrong gestures. Nothing is transmitted. The profiling log has no personal data and no credentials |
| VII | Reproducible, portable | ✅ | ✅ | No new libraries. A LilyGoLib patch, if needed, goes through the existing idempotent `patch_lilygolib.py` (ADR-008). S3 compiles |
| VIII | Simplicity & experiments | ✅ | ✅ | One flush filter instead of re-theming every page. No `RTC_DATA_ATTR` mirror (the NVS save at eject suffices). IMU deep-sleep wake is a time-boxed spike behind `AERODECK_IMU_WAKE`, default off until promoted (R7) |

**Gate: PASS.** Complexity Tracking is not needed.

**SRS amendments in this branch**:
- **FR-EJT-004 note:** timeout steps 10 s / 15 s / 30 s / 1 min / 5 min / Never; the button press that wakes the
  screen is consumed; touches on a dark screen are ignored.
- **NFR-PWR-004 note:** the AXP2101 has no current ADC; the profile logs %, mV and the drain rate.
- **OI-1:** closed by ADR-014 with the spike result.
- **FR-PFD-012 (new):** horizon tilt reference, added at the wearer's request during implementation (spec FR-023).
  It is folded into S7 by the wearer's decision; the top tape stays the seconds because there is no magnetometer.
- **FR-NVG-003 note:** WARNING inverse with 2 Hz blink, CAUTION inverse, ADVISORY outline.

**Spec amendments** (made with this plan):
- FR-018: the battery current is logged where measurable;
- Assumptions: on the watch face, the annunciator moves below the handle while alerts are shown (R8).

**SAS notes**:
- §4 `IBoard` methods (contracts);
- §5.3 eject sequence: hooks, quiesce, own board sequence (R6), no `RTC_DATA_ATTR` mirror;
- §6 `ScreenService` row;
- §7.1 settings v8 and recorder at eject;
- §8 NVG by flush filter (R1).
- **ADR-014:** deep sleep without the vendor `sleep()`; wake sources; OI-1 result.

## Project Structure

### Documentation (this feature)

```text
specs/008-nvg-eject-power/
├── plan.md  research.md  data-model.md  quickstart.md
├── contracts/serial_commands.md  contracts/board_interface.md
├── checklists/requirements.md
└── tasks.md            # /speckit-tasks
```

### Source Code (new or changed)

```text
src/core/
├── ScreenPolicy.{h,cpp}        # on/off decisions, timeout table (pure)
├── RaiseDetector.{h,cpp}       # gravity → raise events (pure)
├── EjectGesture.{h,cpp}        # double tap + pull state machine (pure)
├── NvgColor.{h,cpp}            # RGB565 → green565 via per-channel tables (pure)
├── NvgStyle.{h,cpp}            # alert level × NVG × phase → inverse/visible/outline (pure)
└── SettingDefs.{h,cpp}         # + nvg, scrto, raise; schema 8
src/board/IBoard.h, BoardUltra.cpp, BoardS3.cpp
                                # setDisplayOn, deepSleep(wakeMask, timerSec), wakeCause, setNvgFilter (flush wrap)
src/services/PowerService.{h,cpp}   # eject(), addPreSleepHook(), quiesce handshake, `ad pwr log`, `ad eject`, `ad wake`
src/services/EventBus.h             # + NvgChanged, EjectRequested, ScreenChanged
src/ui/ScreenService.{h,cpp}        # UI task: idle timer, raise feed, display on/off, wake on alert, brightness/NVG apply
src/ui/widgets/EjectHandle.{h,cpp}  # hazard-stripe handle, feeds EjectGesture, posts EjectRequested
src/ui/AnnunciatorBar.{h,cpp}       # NvgStyle rules; setTopOffset() for the watch face
src/pages/PfdPage.{h,cpp}           # hosts EjectHandle; annunciator offset on enter/leave
src/pages/SettingsPage.{h,cpp}      # DISPLAY: NVG on/off, Screen timeout −/+, Raise to wake on/off
src/app/main.cpp                    # ScreenService in the loop; skip lv_timer_handler when off; wake cause log
tools/patch_lilygolib.py            # only if a needed sleep step is not public (AERODECK-PATCH-2)
test/test_screen_policy/ test_raise_detector/ test_eject_gesture/ test_nvg/  (+ test_settings v8)
docs/adr/014-deep-sleep-and-wake-sources.md, docs/power_report.md
docs/software_requirements_specification.md, docs/software_architecture_specification.md, README.md
```

**Structure Decision**: Continue the S0–S6 layout. `ScreenService` sits in `src/ui/`, because it drives the display
and LVGL on the UI task, the same as `PageManager`.

## Implementation Notes (for /speckit-tasks)

1. **Order:**
   1. **Probe first, on the device:**
      - `setDisplayOn(false/true)`: sleep and wake work, the content is intact after waking, how long wake takes;
      - the `deepSleep` sequence without the delay: wake by button, the clock kept, time to sleep, the
        wake cause;
      - the flush filter with a fixed test pattern: the colours look right, FPS.
   2. Core tests → core.
   3. Settings v8.
   4. `IBoard` additions.
   5. `ScreenService` + main loop.
   6. Raise.
   7. `PowerService` eject + `ad` commands.
   8. EjectHandle + annunciator offset.
   9. NVG filter + `NvgStyle` + brightness.
   10. Settings rows.
   11. IMU-wake spike (R7, ≤2 h).
   12. Power profiling and measurements → report.
   13. Docs.
2. **Quiesce handshake:**
   - `PowerService::eject` sets `g_ejecting`.
   - The UI loop sees it, stops calling `lv_timer_handler()`, sets the display off, and gives the semaphore.
   - The svc task waits for it for ≤200 ms, then continues regardless.
   - No LVGL calls from the svc task.
3. **Wake-press consumption:** the board button callback goes to `ScreenService::onButton()` first. If the screen
   was off, it turns it on and returns true (consumed); otherwise `PageManager::buttonPressed()` runs.
4. **Annunciator on the watch face:** `PfdPage::onEnter` calls `AnnunciatorBar::setTopOffset(S(28))`;
   `onLeave` calls `setTopOffset(0)`.
5. **NVG apply:** `applySetting(Nvg)` does three things: `board().setNvgFilter(on)`, re-applies the brightness, and
   posts `NvgChanged`. It is applied in `setup()` before the first frame (FR-014 "from the first frame").
6. **Measure and record:**
   - eject time (gesture end → `esp_deep_sleep_start`, logged as a timestamp just before);
   - wake → PFD time;
   - wake latency for the screen;
   - FPS in NVG;
   - raise hit rate and false wakes;
   - state currents (R9).
7. **No settings reset on the device** without asking (user memory). The Restore-defaults check (FR-022) is
   unit-tested; an on-device test only with consent.
8. **At the end:** update `docs/product_roadmap.md` per the status tracking rule (S7 → ✅ with the PR number;
   record the measured battery life against the M3 exit criterion).

## Complexity Tracking

Not needed. The Constitution Check passed with no violations.
