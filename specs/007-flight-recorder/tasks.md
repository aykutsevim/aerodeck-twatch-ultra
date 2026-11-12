---

description: "Task list for G-Force & Flight Recorder (Slice S6)"
---

# Tasks: G-Force & Flight Recorder (Slice S6)

**Input**: Design documents from `/specs/007-flight-recorder/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/serial_commands.md, quickstart.md

**Tests**: INCLUDED. Constitution Principle V and spec FR-013 require Unity tests for the new `src/core/` modules,
written before the implementation.

**Organization**:
- US1: Flight Miles from steps (P1)
- US2: Max G with its time (P1)
- US3: today's values survive restarts (P1)
- US4: Recorder page with a 7-day chart (P2)
- US5: new day at local midnight (P2)

## Format: `[ID] [P?] [Story] Description`

## Conventions (apply to every task)

- **Running PlatformIO:** `& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio …`. Native tests
  need WinLibs GCC on PATH. Serial tools open COM3 with **DTR/RTS low**; USB CDC can drop log lines, so scripts
  cross-check state with `ad rec` rather than relying on single log lines.
- **Rules from S0–S5 still hold:**
  - SensorLib / LilyGoLib only in `src/board/`;
  - `src/core/` is pure;
  - no LVGL in services; pages read `RecorderSnapshot`;
  - motion callbacks on the UI task do O(1) work only;
  - Snapshot writes come from one task (svc).
- **Write helper scripts and C string edits with the Write/Edit tools** (shell heredocs mangle backslashes).
- **Wearer checks** (walking, flicks, chart readability): ask, and record the answer verbatim in `validation.md`.
- **References:** contracts in `contracts/serial_commands.md`; entities in `data-model.md`; decisions in
  `research.md` R1–R10.

---

## Phase 1: Setup

- [X] T001 Create the test folders `test/test_day_log/`, `test/test_step_acc/`, `test/test_flight_math/` and
  `test/test_rec_blob/`
- [X] T002 Verify the baseline on branch `007-flight-recorder`: `pio test -e native` (184/184),
  `pio run -e twatch_ultra`, `pio run -e twatchs3`. Create `specs/007-flight-recorder/validation.md`.

---

## Phase 2: Foundational (blocking)

**Purpose:** the pure recorder core, settings v7, IBoard motion, and the RecorderService skeleton (counting, no
persistence yet), plus the **motion-source probe** that decides the R1 fallback.

### Tests (write first)

- [X] T003 [P] Write `test/test_day_log/test_main.cpp` for `core::DayLog` (data-model §2):
  - `localDayOf` at +180 min: 2026-11-12T20:59:59Z → local 2026-11-12 (23:59:59); 2026-11-12T21:00:00Z → local
    2026-11-13; negative offsets; floor for times before the epoch;
  - `weekday`: 1970-01-01 → Thursday (3 with Monday = 0); 2026-11-12 → Thursday;
  - `advance`: invalid clock → nothing; undated → dated to the given day (steps kept); next day → today moves to
    history[0], today zeroed; +3 days → 2 empty records inserted; +20 days → all 7 history days empty (the finished day is older than 7 days);
    backward day → nothing; same day → nothing; 8 consecutive rollovers keep 7 (oldest dropped);
  - `addSteps`, `notePeak` (only higher raises; returns true on raise; the minute recorded);
  - `dayAt(0..6)` maps by date (gaps empty).
- [X] T004 [P] Write `test/test_step_acc/test_main.cpp` for `core::StepAccumulator` (data-model §3):
  first reading from 0 at boot; increasing raw → deltas; raw lower than last (sensor restart) → delta = raw;
  implausible jump (> 6 × elapsed s + 20) → rejected, re-baselined, next delta normal; `millis` wrap between readings.
- [X] T005 [P] Write `test/test_flight_math/test_main.cpp` for `core::FlightMath` (data-model §4):
  `nm(2470, 75)` = 1.0002…; `formatNm` → "--", "0.00", "1.00", "9.99", "10.0", "123.4"; `toCentiG` (1.0 g → 100,
  clamp at range → saturated); `formatG` → "--", "1.0", "2.7", "16.0+".
- [X] T006 [P] Write `test/test_rec_blob/test_main.cpp` for `core::RecBlob` (data-model §5): round-trip of a log
  with undated today, empty days and full values; rejects a wrong version, a short buffer, a flipped byte (CRC);
  the encoded size is 100.
- [X] T007 [P] Extend `test/test_settings/test_main.cpp` for schema v7: `kSchemaVersion == 7`; `stride` U8 30..150
  default 75 editable, not secret; migration 6→7 additive; `findSettingKey("stride")`.

### Implementation

- [X] T008 Implement `src/core/DayLog.h/.cpp`, `src/core/StepAccumulator.h/.cpp`, `src/core/FlightMath.h/.cpp` and
  `src/core/RecBlob.h/.cpp` so T003–T006 pass (CRC-16/CCITT-FALSE, little-endian packing)
- [X] T009 Settings v7 in `src/core/SettingDefs.h/.cpp` (`Stride`, schema 7) so T007 passes
- [X] T010 IBoard motion (research R1–R3):
  - `src/board/IBoard.h`: `bool configureMotion(float accelHz)`, `bool stepCount(uint32_t& out)`,
    `bool takePeakG(float& peak, float& last, float& rangeG)` (peak |a| in g since the last call, the latest
    magnitude, the active range; false if unavailable);
  - `src/board/BoardUltra.cpp`: `configureMotion` (I2C lock) enables `STEP_COUNTER` at 1 Hz and `ACCEL_CORRECTED` at
    `accelHz` with `onResultEvent` callbacks; tries `configureRange(BHY2_SENSOR_ID_ACC, 16)`; reads
    `getScaling(BHY2_SENSOR_ID_ACC)`; the step callback stores the count (atomic), the accel callback computes
    `sqrtf(x²+y²+z²) × scale` and keeps the max under a spinlock; `stepCount`/`takePeakG` read them (the latest magnitude feeds
    `RecorderSnapshot.gNow`);
    log `[rec] motion steps=<OK|absent> accel=<OK|absent> scale=<f> range=<g>` once;
  - `src/board/BoardS3.cpp`: all return false.
- [X] T011 `src/services/RecorderService.h/.cpp` skeleton (data-model §7–8):
  - `begin()`: publish an initial snapshot, register `ad rec`, `ad rec sim steps`, `ad rec sim g`, `ad rec clear`
    (no sensor access: the IMU is not started yet);
  - `onImuReady(bool imuOk)`: called from `deferredProbeTask` in `src/app/main.cpp` right after
    `AttitudeService::onImuReady`; `board().configureMotion(50)`; set `stepsOk`/`accelOk`; reset the
    `StepAccumulator` baseline (the hub's count starts at 0 here);
  - `tick(nowMs)` on the svc task at 1 Hz: poll `stepCount` → `StepAccumulator` → `DayLog::addSteps` (log rejected
    jumps); poll `takePeakG` → `toCentiG` → `notePeak(minute of day from ClockSnapshot)` (log `[rec] max g …`);
    `DayLog::advance(clock.valid, localDayOf(utc, tz))` (log `dated`/`rollover`); publish `RecorderSnapshot`;
  - `status()` snapshot accessor.

  Call `begin()` in `src/app/main.cpp` after `TimeService::begin`, register the tick, and add the `onImuReady` call
  in `deferredProbeTask`.
- [X] T012 **Checkpoint on the device (motion probe, R1/R2):**
  - flash; `ad rec` → `steps_src` and `accel` states, `scale`, `range`;
  - at rest `g_now` 1.00 ± 0.05 (if ≈9.8 → the scale is m/s²: divide by 9.80665 in the board and re-check);
  - **ask the user** to walk ~20 steps → `steps` increases by about 20;
  - `ad reboot` → the hardware count restarts at 0 (first reading small), today's in-RAM value is lost (persistence
    comes in US3) — note it.

  If STC is absent: implement the R1 fallback (step detector, one event per step) in `BoardUltra.cpp`, or record
  "steps unavailable" and stop to ask the user. Record everything in `validation.md`.

**Checkpoint:** `pio test -e native` all green, both firmware targets build, steps and g are read on the device.

---

## Phase 3: User Story 1 - Flight Miles from my steps (P1) 🎯 MVP

**Goal:** today's NM on the PFD; stride setting on the watch.

**Independent Test:** quickstart Q2.

- [X] T013 [US1] PFD FLT NM slot in `src/pages/PfdPage.cpp`: read `RecorderService::status()` in the existing tick
  (only when its `seq` changed); text "FLT NM\n<formatNm(nm(steps, stride), stepsOk)>"; keep the S3 corner alignment.
- [X] T014 [P] [US1] Settings row "Stride" in a new **RECORDER** section of `src/pages/SettingsPage.cpp/.h`: −/+
  buttons (5 cm per tap, clamped 30..150) and a "0.75 m" value label, like Brightness; refresh on `SettingsChanged`.
- [X] T015 [US1] Validate quickstart Q2:
  - `ad rec sim steps 2470` → `nm=1.00`, PFD FLT NM 1.00 within 1 s;
  - `ad set stride 80` → today's NM recomputed, steps unchanged; restore 75;
  - **the user** walks a measured ≥500 m route 3 times (stride set to their real stride) and reports the distance →
    NM within ±15 % (SC-001).

  Record in `validation.md`.

---

## Phase 4: User Story 2 - Max G with the time it happened (P1)

**Goal:** today's Max G on the PFD, its time on the recorder state.

**Independent Test:** quickstart Q3.

- [X] T016 [US2] PFD MAX G slot in `src/pages/PfdPage.cpp`: "MAX G\n<formatG(maxGc, accelOk, saturated)>" from the
  snapshot (same tick as T013).
- [X] T017 [US2] Validate quickstart Q3:
  - at rest `g_now` ≈ 1.0;
  - `ad rec sim g 2.5` → MAX G 2.5 and the minute; `ad rec sim g 1.2` → `OK lower`;
  - **the user** makes 10 sharp flicks (`ad rec clear` between, or compare increases) → each above 2.0 within 1 s,
    minute matches (SC-002).

  Record in `validation.md`.

---

## Phase 5: User Story 3 - Today's values survive restarts (P1)

**Goal:** NVS `rec` persistence; Restore defaults keeps the data.

**Independent Test:** quickstart Q4.

- [X] T018 [US3] Persistence in `src/services/RecorderService.cpp` (research R4):
  - `begin()`: open NVS namespace `rec`, read blob `day`, `RecBlob::decode` → DayLog (log `[rec] start …` or
    `load failed (<why>)` → empty);
  - `saveNow(reason)`: encode → `Preferences::putBytes("day", …)` in namespace `rec`, log `[rec] saved (<reason>) <ms>
    ms`, count saves; public for S7;
  - in `tick`: save when dirty and ≥10 min since the last save, and right after a rollover;
  - `esp_register_shutdown_handler` → save (no serial logging inside the handler); if unreliable, call `saveNow`
    from `cmdReboot` in `src/services/SerialConsole.cpp` and note it;
  - `ad rec save` (`OK saved` / `ERR nvs`); `ad rec clear` also erases the blob.
- [X] T019 [US3] Validate quickstart Q4 with a script (`validate_s6.py`, Write tool):
  - 10 × (`sim steps 100`, `sim g 2.0+i/10`, `ad reboot`, `ad rec`) → steps, Max G, its minute and history unchanged
    (SC-003);
  - Settings restore (`ad set reset`) → `ad rec` unchanged, stride 75;
  - power loss: add steps → wait for a periodic save (or `ad rec save`) → add more → **ask the user** to cut power
    (hold the power button ~6 s / unplug) → after restart only the unsaved steps are missing.

  Record in `validation.md`.

---

## Phase 6: User Story 4 - Recorder page with a 7-day chart (P2)

**Goal:** the RECORDER page replaces PAGE B.

**Independent Test:** quickstart Q6.

- [X] T020 [US4] `src/ui/Page.h`: `PageId::PlaceholderB` → `PageId::Recorder` (update every reference).
  `src/pages/RecorderPage.h/.cpp` (research R8):
  - header "RECORDER"; big NM (DSEG7 font as the PFD) + "NM"; rows STEPS, MAX G "2.7 @ 14:05";
  - "Motion sensor unavailable" line when `stepsOk`/`accelOk` is false;
  - 7 bar `lv_obj`s (today green, others cyan, empty days as outlines), heights scaled to the largest NM (each day's NM =
    `FlightMath::nm(steps, snapshot.strideCm)`, so a stride change rescales history too), weekday letters below; tap a bar → its date, NM, steps and Max G in a line above the chart (default: today);
    `noSwipe` not needed for taps, but bars must not block page swipes;
  - refresh in `onTick` when the snapshot `seq` changed; all sizes via `Theme::S()` inside the rounded-corner safe
    area.

  Replace the placeholder registration in `src/app/main.cpp` with the RecorderPage.
- [X] T021 [US4] Validate quickstart Q6: page order PFD → WX → RECORDER → SETTINGS (`ad nav`); 5 × compare page
  values with the PFD and `ad rec` (after `sim steps`/`sim g`); with history (`ad rec rollover` ×3 after adding steps)
  → bars and labels; tap a bar; **the user** judges the chart readable (SC-005).
  Sensor failure: `ad simfail imu` + reboot → PFD FLT NM "--" and MAX G "--", the page shows "Motion sensor
  unavailable", history still visible; then `ad simfail none` + reboot (spec US1-AC5, US2-AC4). Record in
  `validation.md`.

---

## Phase 7: User Story 5 - New day at local midnight (P2)

**Goal:** midnight rollover with the edge rules.

**Independent Test:** quickstart Q5.

- [X] T022 [US5] `ad rec rollover` in `src/services/RecorderService.cpp` (`advance(true, today + 1)`, then save,
  `OK rolled`); the rollover log `[rec] rollover <old> -> <new> (empty=<n>)` in the tick path; make sure a rollover
  publishes immediately (PFD shows 0.00 / 1.0 within 2 s).
- [X] T023 [US5] Validate quickstart Q5 (script):
  - 3 × `ad time set` to 23:59:50 local → `[rec] rollover` within 2 s after midnight, today 0, finished day = `d-1`;
  - clock set back across midnight → no rollover; `ad tz set` to another zone with the same date → none;
    `ad tz set` to a zone where the local date moves forward → exactly one rollover;
  - `ad rec rollover` ×8 → 7 history days, oldest dropped;
  - clock forward 3 days → 2 empty days;
  - restore with `ad sync` (SC-004).

  Record in `validation.md`.

---

## Phase 8: Polish & Cross-Cutting

- [X] T024 [P] Amend `docs/software_requirements_specification.md` and
  `docs/software_architecture_specification.md`:
  - SRS FR-REC-001 note: stride 0.30–1.50 m (setting `stride`, default 0.75), NM always recomputed from steps;
  - SRS FR-REC-005 note: saved every 10 min when changed, at rollover and on restart; **Restore defaults keeps the
    recorder data** (spec FR-015); deep sleep via S7;
  - SAS §4 IBoard motion methods, with the rationale for `stepCount(out)` (reports "unavailable"); §6
    RecorderService row (incl. `onImuReady` after the deferred probe); §7.1 Recorder storage (NVS `rec`, 10 min —
    stricter than the planned hourly); settings v7; §8 RecorderPage replaces PAGE B.
- [X] T025 [P] Update `README.md`: Flight Miles / Max G on the face, the RECORDER page, stride setting, `ad rec …`
  commands with a link to the S6 contract; page ring order.
- [X] T026 Regression and performance:
  - `pio test -e native`; `pio run -e twatchs3`;
  - layering grep (`instance.sensor`/SensorLib only in `src/board/`; `lv_` not in services; `Preferences` only in
    services);
  - S1 regression script; S4 `validate_s4.py order ack`; S5 `validate_s5.py callouts amp`;
  - **SC-006:** PFD + `ad att sweep on` with the recorder running → FPS median ≥28, heap ≥40 KB; save time (ms).

  Record in `validation.md`.
- [X] T027 After the PR is merged, update `docs/product_roadmap.md` per the **status tracking rule**: S6 → ✅ Done
  with the date, the PR number and pending checks; **M2 → ✅** if its definition of done is met (S4, S5, S6 merged).

---

## Dependencies & Execution Order

- **Setup (T001–T002)** → **Foundational (T003–T012)**:
  - tests T003–T007 before T008–T009;
  - T010 (board) before T011 (service) before T012 (probe).
- **US1 (T013–T015)**, **US2 (T016–T017)**: need Foundational; T013 and T016 edit the same file (sequential).
- **US3 (T018–T019)**: needs T011.
- **US4 (T020–T021)**: needs T011; better after US3 so the chart has saved history.
- **US5 (T022–T023)**: needs T011 (and T018 for the save after rollover).
- **Polish:** after the stories. T027 runs only after the merge.

### Parallel Opportunities

- T003 ∥ T004 ∥ T005 ∥ T006 ∥ T007 (test files).
- T014 (SettingsPage) ∥ T013 (PfdPage).
- T020 (RecorderPage) can be written while US3 is validated.
- T024 ∥ T025.

## Parallel Example: tests first

```text
Task: "T003 DayLog test in test/test_day_log/test_main.cpp"
Task: "T004 StepAccumulator test in test/test_step_acc/test_main.cpp"
Task: "T005 FlightMath test in test/test_flight_math/test_main.cpp"
Task: "T006 RecBlob test in test/test_rec_blob/test_main.cpp"
```

## Implementation Strategy

1. **MVP = Foundational + US1 + US2 + US3.** Both numbers on the face, kept across restarts.
2. **US4 page, then US5 rollover validation** (the rollover logic itself is in the Foundational core).
3. **Docs, regression, merge.** Then the roadmap update (T027), closing M2.

## Notes

- **If an installed API differs** from the task text (SensorLib callback signature, `configureRange` units, the
  shutdown-handler behaviour): follow the installed sources and note it in `validation.md`.
- Data stays on the watch (constitution VI); never print anything but counts, dates and g values.
