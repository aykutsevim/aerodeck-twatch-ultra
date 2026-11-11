---

description: "Task list for PFD Watch Face (Slice S1)"
---

# Tasks: PFD Watch Face (Slice S1)

**Input**: Design documents from `/specs/002-pfd-watch-face/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/serial_commands.md, contracts/internal_interfaces.md, quickstart.md

**Tests**: INCLUDED. Constitution Principle V requires Unity tests for every new `src/core/` module, written before the
implementation.

**Organization**: Grouped by user story: US1 horizon · US2 time tapes · US3 EICAS · US4 recorder placeholders.

## Format: `[ID] [P?] [Story] Description`

## Conventions (apply to every task)

- Run PlatformIO as `& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio …`. For native tests, put
  `%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin`
  on PATH (WinLibs GCC). Serial tools must open COM3 with **DTR/RTS low** (otherwise the watch resets).
- S0 rules still hold: vendor types only in `src/board/`; LVGL only on the UI task; `src/core/` is pure C++; sizes via
  `Theme::S()`; never log inside the BHI callback above DEBUG.
- Interfaces: `contracts/internal_interfaces.md`. Serial syntax: `contracts/serial_commands.md`. Layout/entities:
  `data-model.md`. Decisions: `research.md` (R1–R10).

---

## Phase 1: Setup

- [X] T001 Create the folders `src/ui/widgets/`, `src/ui/fonts/`, `test/test_attitude/`, `test/test_lock/`, `test/test_clock_math/`, `test/test_fuel/`, `test/test_pfd_layout/`
- [X] T002 [P] Generate the HUD digit fonts (research R6). Download `fonts-DSEG_v046.zip` from the keshikan/DSEG GitHub release **v0.46** (SIL OFL 1.1) into a temp dir, record its SHA-256, extract `DSEG7Classic-Bold.ttf`, then run `npx lv_font_conv@1.5.3 --font DSEG7Classic-Bold.ttf --range 0x20,0x2D,0x2E,0x30-0x3A --size <N> --bpp 4 --format lvgl --no-compress --lv-include lvgl.h -o src/ui/fonts/dseg7_<N>.c` for N=40 and N=28 (symbols `dseg7_40`, `dseg7_28`). Add `src/ui/fonts/OFL.txt` (the licence text from the release) and `src/ui/fonts/fonts.h` declaring `LV_FONT_DECLARE(dseg7_40)` and `LV_FONT_DECLARE(dseg7_28)`. Record the exact commands, the release tag v0.46, the zip SHA-256 and `lv_font_conv@1.5.3` (verified as published on npm) in `tools/gen_fonts.md`. If the download or npx is blocked, skip this task, note it in `specs/002-pfd-watch-face/validation.md`, and use Montserrat 40/28 in T037/T040 (fallback in R6)
- [X] T003 Verify the build: `pio run -e twatch_ultra` succeeds with the font files compiled in (they are unreferenced, so the linker drops them)

---

## Phase 2: Foundational (blocking)

**Purpose**: HAL access to the BHI260AP gravity sensor and the RTC, and a PFD page shell replacing the S0 Home placeholder.

- [X] T004 Extend `src/board/IBoard.h` per `contracts/internal_interfaces.md`: `struct GravitySample`, `GravityCb`, `virtual bool configureAttitude(float rateHz, GravityCb cb, void* ctx)`, `virtual bool readRtc(std::tm& utc, bool& integrity)`, `virtual bool writeRtc(const std::tm& utc)` (include `<ctime>`)
- [X] T005 Implement them in `src/board/BoardUltra.cpp`:
  - **`configureAttitude`**: return false unless `periph(Periph::Imu) == Ok`. Under `I2cLock`:
    - `instance.sensor.onResultEvent(BHY2_SENSOR_ID_GRA, &BoardUltra::onGravity, this)`;
    - `instance.sensor.configure(BHY2_SENSOR_ID_GRA, rateHz, 0)`.
  - **`onGravity`** (static, matching SensorLib's `SensorDataParseCallback(uint8_t id, uint8_t* data, uint32_t size, uint64_t* ts, void* user)`):
    - parse the 3 axes with SensorLib's helper (`bhy2_parse_xyz` from `bosch/bhy2_parse.h`, or `BoschSensorDataHelper`); use the scaling from `instance.sensor.getScaling(BHY2_SENSOR_ID_GRA)` if available, else 1/4096 g per LSB — check both in `.pio/libdeps/twatch_ultra/SensorLib/src`;
    - fill `GravitySample{x, y, z, millis()}` and call the stored cb.
    - Note: S1 only needs the *ratio* of the axes (pitch/roll), so a wrong absolute scale doesn't affect it. Slice S6
      (peak g) must confirm the absolute scale and record it.
  - **`readRtc`**: `I2cLock`; `RTC_DateTime d = instance.rtc.getDateTime()` → `std::tm`; `integrity = instance.rtc.isClockIntegrityGuaranteed()`; return false if `periph(Periph::Rtc) != Ok`.
  - **`writeRtc`**: `I2cLock`; `instance.rtc.setDateTime(RTC_DateTime(year, month, day, hour, minute, second, weekday))`.
- [X] T006 [P] Implement the same methods in `src/board/BoardS3.cpp`: `configureAttitude` returns false; `readRtc`/`writeRtc` use `instance.rtc` (PCF8563, same SensorLib `RTC_DateTime` API; check the member name in `LilyGoWatchS3.h`). It must compile
- [X] T007 **Layout first, test-first (FR-018):**
  - write `test/test_pfd_layout/test_main.cpp` for a pure `core::PfdLayout::compute(int w, int h)` returning the boxes of data-model §9 (heading, adi, hour, minute, zulu, local, eicas, maxg, nm). It asserts, for **410×502 (Ultra, portrait), 502×410 and 240×240**: every box lies inside the screen, no two boxes overlap, the ADI is square and centred, and tape and corner boxes are ≥24 px tall at 502×410;
  - implement `src/core/PfdLayout.h/.cpp` so it passes.

  Then create `src/pages/PfdPage.h/.cpp` (`PageId::Home`, title "PFD") with the regions from `core::PfdLayout::compute(board w, h)` as empty child containers: `headingArea_`, `adiArea_`, `hourArea_`, `minuteArea_`, `zuluLbl_`, `localLbl_`, `eicasArea_`, `maxgLbl_`, `nmLbl_`. `onEnter` **first forces a full refresh of every widget from the current clock, attitude and power data (FR-021: no stale values after navigating back)**, then creates a 16 ms `lv_timer` (`frame_`). `onExit` deletes it (R10). Replace `HomePage` with `PfdPage` in `src/app/main.cpp`, and delete `src/pages/HomePage.h/.cpp`
- [X] T008 Build + flash. Verify S0 behaviour is intact: `ad nav` ring, `ad periph`, caution banner, `boot->home` ≤4 s. Record in `specs/002-pfd-watch-face/validation.md`

**Checkpoint**: The PFD shell is the home page. HAL can reach the BHI gravity sensor and the RTC.

---

## Phase 3: User Story 1 - Artificial horizon (P1) 🎯 MVP

**Goal**: Horizon tracks the wrist at ≥30 FPS, with LOCK and ATT ALIGN/FAIL flags.

**Independent Test**: quickstart P3 + P4 (angles ±3° vs a phone level, LOCK 10/10, latency ≤50 ms, FPS ≥30 for 60 s).

### Tests for User Story 1 (write first)

- [X] T009 [P] [US1] `test/test_attitude/test_main.cpp` for `core::Attitude`:
  - g=(0,0,1) → pitch 0, roll 0;
  - g=(sin30,0,cos30) → roll +30;
  - g=(0,−sin10,cos10) → pitch +10;
  - near vertical (|pitch|>80°) roll is held at the previous value;
  - EMA converges within 5 updates to within 0.5°;
  - roll wrap-around across ±180 does not jump the EMA;
  - `reset()` clears state;
  - `axisMap` is applied (test with a non-identity map).
- [X] T010 [P] [US1] `test/test_lock/test_main.cpp` for `core::LockDetector`:
  - in-band for 499 ms → not locked; 500 ms → locked;
  - small tip (4°) keeps the lock; >5° unlocks immediately;
  - re-entering the band restarts the 500 ms timer;
  - `statusOk=false` forces unlock.

### Implementation for User Story 1

- [X] T011 [US1] Implement `src/core/Attitude.h/.cpp` per data-model §1 (constexpr `AxisMap{int8 sx, sy, sz; uint8 ix, iy, iz}` default identity, settable) so T009 passes
- [X] T012 [US1] Implement `src/core/LockDetector.h/.cpp` per data-model §2 so T010 passes. Run `pio test -e native` (all suites green)
- [X] T013 [US1] Implement `src/services/AttitudeService.h/.cpp`:
  - `Snapshot<AttitudeSnapshot>`;
  - `onImuReady(bool ok)` → `board().configureAttitude(100, cb, nullptr)`; status Align → Ok/Fail; publish `AttitudeStatusChanged` (add it to `EventType`);
  - gravity cb (UI task) → `core::Attitude` → `LockDetector` → snapshot write (`sampleMs`, `seq++`);
  - watchdog: status Fail if no sample for >1 s while Ok;
  - latency stats (last/max over 10 s, research R9) fed by `notifyRendered(sampleMs)`;
  - serial `ad att` per contract.
  Call `AttitudeService::onImuReady(board().periph(Periph::Imu) == PeriphState::Ok)` at the end of `deferredProbeTask` in `src/app/main.cpp`. Because that task runs on core 0, `configureAttitude` takes the I2C lock internally.
- [ ] T014 [US1] **Spike T-ATT (axis mapping).** Flash and use `ad att` with the watch flat, then rolled right ~30° (right edge down when worn), then pitched up ~10° (top edge up). Set the `AxisMap` constant in `src/services/AttitudeService.cpp` so that flat = (0,0), right roll = +roll and nose-up = +pitch. Record the raw gravity values and the chosen mapping in `specs/002-pfd-watch-face/validation.md`
- [X] T015 [US1] Implement `src/ui/widgets/AdiWidget.h/.cpp` (research R2). `create(parent, size)` makes one `lv_obj` (no children, clip corner, sky colour bg) with an `LV_EVENT_DRAW_MAIN` handler that draws via `lv_event_get_layer(e)`:
  - ground polygon: compute the horizon line from roll/pitch (pixels per degree = size/60), intersect it with the square, and fill the ground side with `lv_draw_triangle` (split the polygon into ≤2 triangles);
  - pitch ladder: `lv_draw_line` every 5° within ±25° (long lines at 10° with numeric labels via `lv_draw_label`, short lines at 5°);
  - fixed aircraft symbol (yellow W shape: 3 lines + centre dot);
  - roll index triangle at the top.

  `set(const AttitudeSnapshot&)` stores the values and calls `lv_obj_invalidate` only if |Δpitch| or |Δroll| ≥0.2° or the lock/status changed. A `LOCK` label (green, hidden unless `level`) and an `ATT ALIGN`/`ATT FAIL` flag label (amber/red, centred) are siblings placed over the widget.
- [X] T016 [US1] **Spike T-PERF (gate).**
  - Put `AdiWidget` (size `S(164)` ≈ 280 px) in `adiArea_` of `PfdPage`; the frame timer calls `adi.set(AttitudeService::att().read())` and `AttitudeService::notifyRendered(...)` on `LV_EVENT_RENDER_READY`.
  - Flash, enable the overlay (`ad set overlay 1`), move the wrist continuously for 60 s, and sample `ad stat` every 2 s.
  - **Gate: overlay FPS ≥30 sustained.** If missed, apply research R2 options (a) shrink to `S(140)`, (b) no AA on the ladder, (c) 0.5° quantisation, re-measuring after each.
  - **If still <30 FPS: STOP and ask the user** before option (d) (LilyGoLib DMA buffer patch).
  - Record all measurements in `validation.md` and in ADR-011 (T046).
- [X] T017 [US1] Edge behaviour in `AdiWidget`: clamp the drawn pitch to ±30° (full sky/ground beyond), keep the horizon inside the square, and show ATT ALIGN while status=Align (first ~3 s after boot) and ATT FAIL (level picture) while status=Fail
- [ ] T018 [US1] Validate quickstart P3 (all items incl. `ad simfail imu`) and P4 FPS. Record angles vs the phone level, LOCK 10/10 trials, `lat_ms`/`lat_max_ms`, and FPS in `validation.md`

**Checkpoint**: Horizon MVP works on the watch.

---

## Phase 4: User Story 2 - Time on the tapes (P1)

**Goal**: Hour/minute/heading tapes and ZULU/LOCAL from a UTC RTC + `tzoff`; CLOCK NOT SET handling; serial clock set.

**Independent Test**: quickstart P2 + P5 (10-min reference comparison, 23:59:59 rollover, tzoff 330 / 335).

### Tests for User Story 2 (write first)

- [X] T019 [P] [US2] `test/test_clock_math/test_main.cpp` for `core::ClockMath`:
  - `splitLocal` at 14:37:20.000 UTC+0 → 14/37/20, heading 120.0;
  - ms=500 → heading 123.0;
  - tz +180 crossing midnight (21:00Z → 00:00 local);
  - tz −300;
  - tz +330 (half hour);
  - 59 s + 999 ms → heading 359.994 (no wrap jump to 360);
  - `parseIso8601Utc` valid / missing Z / bad month 13 / Feb 30 / leap day 2028-02-29;
  - `clockPlausible(2024)` false, `(2026)` true.
- [X] T020 [P] [US2] Extend `test/test_settings/test_main.cpp`:
  - `tzoff` def 180, min −720, max 840, step 30;
  - `validateSetting(TzOff, 330)` true, `335` false, `−750` false;
  - `kSchemaVersion == 2`;
  - `schemaAction(1, 2, true) == Migrate`.

### Implementation for User Story 2

- [X] T021 [US2] Implement `src/core/ClockMath.h/.cpp` (pure; days-from-civil algorithm, no `gmtime` dependency) so T019 passes
- [X] T022 [US2] Update `src/core/SettingDefs.h/.cpp`: add the `int32_t step` field to `SettingDef` (0 = any; update the S0 table entries with step 0), add `SettingKey::TzOff` `{"tzoff", I32, 180, −720, 840, step 30, editable}`, enforce the step in `validateSetting`, set `kSchemaVersion = 2`. Update `formatSettingForOutput` only if needed, and the `ad set` out-of-range message in `src/services/SettingsStore.cpp` to include `step N` when step>0. Run `pio test -e native` (T020 + S0 suites green)
- [X] T023 [US2] Verify migration in `src/services/SettingsStore.cpp`: the S0 Migrate branch already writes the new schema. Ensure missing keys get defaults written (write the sanitised cache for keys not present). Flash over the S0 settings: `ad set list` shows `tzoff=180`, the old values are kept, and the log shows `migrating settings schema 1 -> 2`
- [X] T024 [US2] Implement `src/services/TimeService.h/.cpp`:
  - `begin()`: `board().readRtc` → validity (integrity && `clockPlausible(year)`) → `settimeofday`.
  - `Snapshot<ClockSnapshot>` with `valid`, `utc` and `tzOffsetMin` (from settings; refreshed on `SettingsChanged`); 1 Hz svc tick.
  - Raise/clear `CautionId::ClockNotSet` "CLOCK NOT SET" (add it to `Cautions.h`).
  - `set(utc)` writes the RTC + `settimeofday`, clears the caution and publishes `ClockChanged` (add it to `EventType`).
  - Serial `ad time` and `ad time set` per contract.
  - Call `begin()` in `src/app/main.cpp` after `board().begin`.
- [X] T025 [US2] Implement `src/ui/widgets/TapeWidget.h/.cpp` (research R3):
  - **Vertical**: clip container, inner strip of 5 labels (value−2…value+2) in the body font, a fixed value box with a border and the big font; `setValue(int)` rewrites the labels only on change.
  - **Horizontal (heading)**: inner strip with ticks every 5° and numeric labels every 30° spanning ±60°; `setHeading(float deg)` positions the strip with sub-pixel-free integer `lv_obj_set_x` from the fractional degrees. The value box shows `%03d` of whole degrees.
  - Invalidate only the tape.
- [X] T026 [US2] Integrate into `src/pages/PfdPage.cpp`:
  - hour tape in `hourArea_`, minute tape in `minuteArea_`, heading tape in `headingArea_`;
  - ZULU/LOCAL labels `ZULU 14:37` / `LOCAL 17:37` with the `dseg7_28` font (or the Montserrat fallback) in hudGreen;
  - frame timer: `gettimeofday` → `core::splitLocal` → `heading.setHeading` every frame; hour/minute/corners only when the minute changes;
  - when `ClockSnapshot.valid == false`, show `--` everywhere and freeze the heading at `---`.
- [X] T027 [US2] Add a "Local offset" row to `src/pages/SettingsPage.cpp` (−/+ buttons, step 30 min, label `UTC+03:00` / `UTC-05:30`), using `SettingsStore::set(SettingKey::TzOff, …)`
- [X] T028 [US2] Re-measure quickstart P4 FPS with the per-frame heading tape added (gate ≥30 FPS; if it fails, apply R2 options before continuing). Then validate quickstart P2 + P5: set the clock, check across a reboot, the rollover at `2026-11-11T20:59:55Z` (tz 180 → local 23:59:55→00:00:00), the 10-min comparison against a reference clock (via serial `ad time` plus the display), and `ad set tzoff 330/335`. Record in `validation.md`

**Checkpoint**: The PFD tells the time.

---

## Phase 5: User Story 3 - EICAS fuel gauges (P2)

**Goal**: FUEL QTY, FUEL FLOW (%/h), EGT, CHG annunciator, LOW FUEL caution.

**Independent Test**: quickstart P6.

### Tests for User Story 3 (write first)

- [X] T029 [P] [US3] `test/test_fuel/test_main.cpp`:
  - `FuelFlow`: NaN before a 10-min span; a constant 1 %/10 min drain → ≈6 %/h (±0.5); noise of ±1 % around the trend still gives ±1.5 %/h at 30 samples; reset on a charging toggle; reset on a >5 % jump; the ring keeps the last 30.
  - `FuelState`: 15 → not low; 14 → low; back at 16 → still low; 18 → clear; charging clears immediately.

### Implementation for User Story 3

- [X] T030 [US3] Implement `src/core/Fuel.h/.cpp` (`FuelFlow`, `FuelState`) per data-model §4 so T029 passes
- [X] T031 [US3] Extend `src/services/PowerService.h/.cpp`:
  - `PowerSnapshot` += `fuelFlowPctPerH` (NaN = n/a) and `lowFuel`;
  - feed `FuelFlow` once per minute (`nowMs/60000`) and `FuelState` every poll;
  - raise/clear `CautionId::LowFuel` "LOW FUEL" (add it to `Cautions.h`);
  - serial `ad fuel` per contract, plus a test hook `ad fuel sim <pct|off>` that overrides `battPct` for FuelState/EICAS (documented in the contract as a test hook).
- [X] T032 [US3] Implement `src/ui/widgets/EicasWidget.h/.cpp`:
  - three cells (FUEL QTY `82%`, FUEL FLOW `4.2%/h`, `--`, or `REFUEL` when charging, EGT `31°C`) and a `CHG` annunciator (cyan, hidden unless charging);
  - small caption labels above the values (`FUEL QTY`, `FUEL FLOW`, `EGT`);
  - `--` when `PowerSnapshot.valid == false`;
  - `set()` updates only changed labels.
- [X] T033 [US3] Integrate the EICAS into `eicasArea_` of `PfdPage` (update at 1 Hz from the frame timer when `PowerService::power().seq()` changes)
- [ ] T034 [US3] Validate quickstart P6 (10 samples vs `ad fuel`, USB plug/unplug ×5 timing, `ad fuel sim 14/16/18` hysteresis + caution, FLOW `--` then a value after 10 min on battery). Record in `validation.md`

**Checkpoint**: EICAS is live.

---

## Phase 6: User Story 4 - Recorder placeholders (P3)

- [X] T035 [US4] In `src/pages/PfdPage.cpp`, show the `maxgLbl_` "MAX G --" (top-left) and `nmLbl_` "FLIGHT NM --" (top-right) in the small font, dim colour, positioned so they don't overlap the heading tape. Add a comment that S6 fills them
- [X] T036 [US4] Verify both labels are visible on the device and record in `validation.md`

---

## Phase 7: Polish & Cross-Cutting

- [X] T037 [P] Final layout pass in `src/pages/PfdPage.cpp`: check no overlaps on the device at 410×502 (the host test in T007 covers 240×240 too), digits ≥24 px tall (FR-019), and that the caution banner and page indicator don't cover the hour/minute boxes permanently
- [X] T038 [P] Write `docs/adr/010-attitude-from-bhi-gravity.md` (context, decision, delivery on the UI task through LilyGoLib's loop, the axis mapping found in T014, consequences, alternatives)
- [X] T039 [P] Write `docs/adr/011-custom-draw-horizon.md` (S0 FPS data, the T016 measurements, the options applied, the final ADI size)
- [X] T040 Amend `docs/software_requirements_specification.md` (constitution Workflow §2: new requirements get SRS IDs in this branch):
  - FR-PFD-007 → "drain rate (%/h) estimated from the battery-percentage trend (AXP2101 has no battery-current ADC)";
  - FR-TIM-003 → add "until S2, local time = UTC + fixed `tzoff` minutes (−12:00…+14:00, 30-min steps), editable on the Settings page";
  - **new FR-PFD-011** (M): "While attitude is initialising the ADI shows ATT ALIGN; if motion sensing is unavailable it shows a level picture with ATT FAIL, and all other PFD functions keep working";
  - **new FR-TIM-006** (M): "If the RTC lost its time or holds a year < 2025, all time readouts show '--' and a CLOCK NOT SET caution is raised";
  - **new FR-TIM-007** (M, interim): "Until NTP (S2), the UTC clock can be set via the diagnostic serial console and is kept in the RTC across reboots";
  - add the new IDs to the SRS §6 traceability row for S1. Amend `docs/software_architecture_specification.md`: §5.1 attitude row → ADR-010, §8 horizon → ADR-011, §11 ADR rows 010/011
- [X] T041 [P] Update `README.md`: PFD description, the new `ad time`/`ad att`/`ad fuel` commands, and setting the clock before first use
- [X] T042 Regression and portability: `pio test -e native` (all suites), `pio run -e twatchs3`, the layering grep from S0 T062 (no vendor includes outside `src/board/`, no `lv_` in core/services, no 502/410 literals outside board). Fix any issues
- [ ] T043 Run quickstart P1–P7 end to end. This explicitly includes **P4 on the complete PFD** (60 s of wrist motion, overlay FPS ≥30, SC-001) and the SC-008 hallway test (≥3 people read hour and minute, time each). Complete `specs/002-pfd-watch-face/validation.md` with SC-001…SC-009 results and tick the constitution merge gates

---

## Dependencies & Execution Order

- **Setup (T001–T003)** → **Foundational (T004–T008)** → user stories.
- **US1**: T009/T010 → T011/T012 → T013 → T014 (spike) → T015 → **T016 gate** → T017 → T018.
- **US2**: T019/T020 → T021/T022 → T023 → T024 → T025 → T026 → T027 → T028. Independent of US1 except for sharing
  `PfdPage` (sequential edits of the same file).
- **US3**: T029 → T030 → T031 → T032 → T033 → T034. Independent of US1/US2 (same `PfdPage` caveat).
- **US4**: after T007.
- **Polish**: after the stories. T039 needs T016 results; T038 needs T014 results.
- **Hard gate**: if T016 fails all cheap options, the remaining UI tasks pause for a user decision.

### Parallel Opportunities

- T002 alongside Phase 2.
- Test-first pairs: T009 ∥ T010, T019 ∥ T020, T029 (all different files).
- T006 (S3 board) ∥ T005.
- Core modules of different stories (T011/T012, T021/T022, T030) can be built in parallel once their tests exist.
- Docs T038/T039/T041 in parallel.

## Parallel Example: tests first

```text
Task: "T009 [US1] test/test_attitude/test_main.cpp"
Task: "T010 [US1] test/test_lock/test_main.cpp"
Task: "T019 [US2] test/test_clock_math/test_main.cpp"
Task: "T029 [US3] test/test_fuel/test_main.cpp"
```

## Implementation Strategy

1. **MVP = US1** (horizon with the FPS gate). It retires the biggest risk first.
2. Then US2 (time). The PFD is now a usable watch → demo.
3. US3 EICAS, US4 placeholders, polish → merge (milestone M1 still needs S2/S3).

## Notes

- `validation.md` is created at T008 and completed at T043.
- If a SensorLib/LilyGoLib API name differs from what a task names, follow the installed source and note it in `validation.md`.
- The BHI callback runs on the UI task while the I2C lock is held. Keep it allocation-free and short.
