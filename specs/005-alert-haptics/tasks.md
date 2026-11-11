---

description: "Task list for Alert Manager & Haptics (Slice S4)"
---

# Tasks: Alert Manager & Haptics (Slice S4)

**Input**: Design documents from `/specs/005-alert-haptics/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/serial_commands.md,
contracts/annunciator_ui.md, quickstart.md

**Tests**: INCLUDED. Constitution Principle V requires Unity tests for new `src/core/` modules, written before the
implementation (spec FR-016).

**Organization**:
- US1: master annunciator (P1)
- US2: STALL / stick shaker (P1)
- US3: acknowledge (P1)
- US4: switch clicks (P2)
- US5: vibration strength and quiet mode (P2)

## Format: `[ID] [P?] [Story] Description`

## Conventions (apply to every task)

- **Running PlatformIO:** `& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio …`. Native tests
  need WinLibs GCC on PATH. Serial tools open COM3 with **DTR/RTS low**.
- **Rules from S0–S3 still hold:**
  - DRV2605/SensorLib only in `src/board/`;
  - every I2C access under `I2cLock`;
  - LVGL only on the UI task (`src/ui/`, `src/pages/`);
  - `src/core/` is pure;
  - sizes via `Theme::S()`;
  - the layout stays inside the 64 px rounded corners.
- **Haptic feel** can only be judged by the wearer. Tasks that need it say so, and record the user's answer in
  `validation.md`.
- **Write helper scripts with the Write tool** (shell heredocs mangle `\\` on this machine).
- **References:** contracts in `contracts/`; entities in `data-model.md`; decisions in `research.md` R1–R8.

---

## Phase 1: Setup

- [X] T001 Create the test folders `test/test_alerts/`, `test/test_battery_alarm/` and `test/test_haptic_policy/`
- [X] T002 Verify the baseline on branch `005-alert-haptics`: `pio test -e native` (142/142), `pio run -e twatch_ultra`
  and `pio run -e twatchs3` succeed. Create `specs/005-alert-haptics/validation.md`.

---

## Phase 2: Foundational (blocking)

**Purpose:** pure alert/battery/haptic logic, settings v5, the IBoard haptic hook, and HapticService.

### Tests (write first)

- [X] T003 [P] Write `test/test_alerts/test_main.cpp` for `core::AlertSet` and `core::AlertTable` (data-model §1–2):
  - order by level desc then raise sequence (Warning > Caution > Advisory; equal level → raised first is first);
  - 10 random raise orders of 5 mixed alerts always put the WARNING master first (SC-002);
  - `raise` of the same id/level/text → `Same`; changed text → `Updated` (acked kept); higher level → `Rearmed`
    (acked cleared);
  - `clear` → removed, and the next becomes master; clearing an absent id → false;
  - `ackAll` returns the newly acknowledged count; `anyUnacked`/`anyUnackedWarning`;
  - a cleared-then-raised id is a new occurrence (unacked, new sequence);
  - capacity 24 → the 25th returns `Full`;
  - `sorted(out, 16)` truncates while `count()` stays the total;
  - table: Stall/PullUp are Warning + StickShaker + 2000 ms; TimeUnsync is Advisory; ClockNotSet is Caution;
    peripheral range 0..12 is Caution with the "`<NAME>` FAIL" text; an unknown id → nullptr;
  - names round-trip (`alertIdFromName("stall")`);
  - `ack(id)` acknowledges only that alert (others stay unacked) and returns false for an absent id;
    `raise` + `ack` (the de-escalated STALL) leaves `anyUnackedWarning() == false`;
  - a text-only change → `Updated`, `acked` unchanged, no re-arm (spec Edge Cases).
- [X] T004 [P] Write `test/test_battery_alarm/test_main.cpp` for `core::BatteryAlarm` (data-model §3):
  - 16 → 14 nochg → raiseStall; 14 → 15 → 16 → no change (hysteresis);
  - → 17 clearAll;
  - 14 → 5 raisePullUp; 5 → 6 no change; → 7 raiseStallAcked; → 17 clearAll;
  - starting at 3 nochg → raisePullUp directly;
  - charging at 3 → clearAll and stays Normal while charging;
  - an invalid reading changes nothing;
  - the sweep 20 → 3 → 20 produces exactly: raiseStall@14, raisePullUp@5, raiseStallAcked@7, clearAll@17 (SC-001).
- [X] T005 [P] Write `test/test_haptic_policy/test_main.cpp` for `core::HapticPolicy` (data-model §4, research R2):
  - the full matrix Pattern × Intensity{Off,Low,High} × quiet{0,1} → play/none and the expected effect ids
    (Click off → none; Click with quiet → still plays; Caution/Advisory with Off or quiet → none; StickShaker →
    always, full strength, same sequence for every intensity);
  - sequences end with ≤8 entries; StickShaker has 3 pulses with waits (`0x80|10`);
  - `shouldStart`: lower prio while the current is running → false; after the current ends → true; equal/higher →
    true.
- [X] T006 [P] Extend `test/test_settings/test_main.cpp` for schema v5:
  - `kSchemaVersion == 5`;
  - `hapticlvl` U8 0..2 default 1, editable;
  - `quiet` Bool default 0, editable;
  - migration 4→5 is additive.

### Implementation

- [X] T007 Implement `src/core/Alerts.h/.cpp` (levels, ids, constexpr table, `alertDef`, `alertIdFromName`,
  `levelName`) and `src/core/AlertSet.h/.cpp` so T003 passes
- [X] T008 [P] Implement `src/core/BatteryAlarm.h/.cpp` so T004 passes
- [X] T009 [P] Implement `src/core/HapticPolicy.h/.cpp` so T005 passes
- [X] T010 Settings v5 in `src/core/SettingDefs.h/.cpp` (`HapticLvl`, `Quiet`, schema 5) so T006 passes. Check that the
  existing migration path in `src/services/SettingsStore.cpp` persists the new defaults.
- [X] T011 IBoard haptic hook:
  - `src/board/IBoard.h`: `virtual bool haptic(const uint8_t* seq, uint8_t n)` and `virtual void hapticStop()`;
  - implement in `src/board/BoardUltra.cpp` and `src/board/BoardS3.cpp` via `instance.drv` under `I2cLock`:
    `setWaveform(i, seq[i])` for i < n ≤ 7, `setWaveform(n, 0)`, `run()`; `hapticStop` → `drv.stop()`;
  - return false unless `periph(Periph::Haptics) == Ok`.
- [X] T012 `src/services/HapticService.h/.cpp`:
  - API: `begin()`, `play(Pattern)`, `click()`, `stopWarning()`;
  - `play`/`click`: `HapticPolicy::select` (current `hapticlvl`/`quiet`) → `HapticPolicy::shouldStart` against a
    mutex-guarded `{curPrio, curEndMs}` → `board().haptic(seq)`, then update `{curPrio, curEndMs = now + lengthMs}`;
  - `stopWarning()`: if `curPrio` is the stick shaker → `board().hapticStop()` and reset the state;
  - `click()` may run on the UI task;
  - serial `ad haptic <click|advisory|caution|shaker|stop>` (contracts/serial_commands.md);
  - DEBUG logs `[hap] …`.

  Call `begin()` in `src/app/main.cpp` after the board starts.
- [X] T013 **Checkpoint on the device:**
  - flash; `ad haptic click|advisory|caution|shaker` at `hapticlvl` 1 and 2;
  - **ask the user** to feel each one and confirm they are distinct, and that low is weaker than high;
  - measure each pattern's real length (poll the DRV2605 GO bit via a temporary board debug, or time it by ear and
    feel) and update the `lengthMs` values in `src/core/HapticPolicy.cpp`;
  - record the effect ids and lengths in `validation.md`. Tune the ids if the user asks.

**Checkpoint:** `pio test -e native` all green, and both firmware targets build.

---

## Phase 3: User Story 1 - One master annunciator for everything (P1) 🎯 MVP

**Goal:** a single alert system with a top band on every page. All existing cautions use it.

**Independent Test:** quickstart Q2 + Q3.

- [X] T014 [US1] `src/services/AlertManager.h/.cpp` (research R4, data-model §5, §7):
  - `begin()`; `raise(id[, level, text])`; `clear(id)`; `ackAll()`; `status()` → `Snapshot<AlertSnapshot>`;
  - `tick()` on the svc task: plays the pattern for `Added`/`Rearmed` via HapticService, and the WARNING repeat every
    2000 ms while `anyUnackedWarning`;
  - events `AlertRaised`/`AlertCleared`/`AlertAcked` in `src/services/EventBus.h` (remove `CautionChanged`);
  - logs `[alert] raise|clear|ack …`;
  - serial `ad alert`, `ad alert raise <id> [W|C|A] ["text"]`, `ad alert clear <id>`, `ad alert ack`
    (contracts/serial_commands.md).

  `begin()` and the svc tick are registered in `src/app/main.cpp`.
- [X] T015 [US1] Retrofit every caution source to AlertManager and delete `src/services/Cautions.h/.cpp`:
  - `src/app/main.cpp`: peripheral failures (`PeriphFirst + i` with "`<NAME>` FAIL"), settings reset/storage error,
    safe mode;
  - `src/services/TimeService.cpp`: CLOCK NOT SET;
  - `src/services/NetService.cpp`: TIME UNSYNC (Advisory);
  - `src/services/PowerService.cpp`: remove LOW FUEL (replaced in US2).

  Grep that no `Cautions::` or `CautionId` remains.
- [X] T016 [US1] `src/ui/AnnunciatorBar.h/.cpp` replaces `src/ui/CautionBanner.h/.cpp` (delete it) per
  contracts/annunciator_ui.md:
  - `lv_layer_top()`, **S(28)** high (a drop-in for the S0 banner), level colours;
  - `<text>  +N`; flashing at 1 Hz (500 ms opacity toggle) while `anyUnacked`; steady cycling every 3 s
    (`n/N`) otherwise; hidden at zero;
  - updates on the snapshot seq (polled from the UI loop/timer) within 1 s;
  - label width limited to the safe width at the band centre, `LV_LABEL_LONG_DOT`.

  Wire it in `src/app/main.cpp` in place of `CautionBanner`.
- [X] T017 [US1] Validate quickstart Q2 (migration, retrofit: IMU FAIL simfail, CLOCK NOT SET, TIME UNSYNC) and Q3
  (10 random orders → WARNING master; ack → cycling; clear one by one). **Ask the user** to confirm the band's look
  on the PFD, WX and Settings. Record in `validation.md`.

---

## Phase 4: User Story 2 - STALL and stick shaker (P1)

**Goal:** the battery below 15 % (not charging) raises STALL with a repeating stick shaker; ≤5 % raises PULL UP;
hysteresis; charging clears.

**Independent Test:** quickstart Q4.

- [X] T018 [US2] `src/services/PowerService.cpp`:
  - replace the LOW FUEL caution logic with `core::BatteryAlarm` on every valid poll → `AlertManager::raise/clear`
    for Stall/PullUp; `raiseStallAcked` → `AlertManager::raiseAcked(Stall)` (raise + `AlertSet::ack(id)` under one
    lock, no pattern played);
  - extend `ad fuel sim <pct|off> [chg|nochg]` with a charging override fed to the alarm and EICAS;
  - keep `FuelState` for the EICAS gauge colour.
- [X] T019 [US2] Validate quickstart Q4: the sweep 20 → 3 → 20 with `nochg`, then `chg`, ×3 (script, log-checked):
  - exactly one STALL raise at 14, PULL UP at 5, STALL (acked) at 7, clear at 17;
  - no flicker;
  - `chg` clears within 2 s.

  `hapticlvl 0` → STALL still vibrates (**the user feels it**).

  **Boot with low battery** (spec Edge Cases): `ad fuel sim 10 nochg`, then `ad reboot`. The sim override resets on
  boot, so re-send it within 2 s of the serial port reopening. STALL appears after the watch face (log order
  `boot->home` before `[alert] raise WARNING STALL`).

  Record SC-001 in `validation.md`.

---

## Phase 5: User Story 3 - Acknowledge to stop the nagging (P1)

**Goal:** a tap on the band acknowledges all and stops repeats; new or escalated alerts re-arm.

**Independent Test:** quickstart Q5.

- [X] T020 [US3] Acknowledge path:
  - AnnunciatorBar is clickable with `GESTURE_BUBBLE` removed (the tap is consumed);
  - `LV_EVENT_CLICKED` → `AlertManager::ackAll()` + `HapticService::click()`;
  - `AlertManager::ackAll()` → `HapticService::stopWarning()` (`drv.stop()` if a WARNING is playing), the band turns
    steady ≤0.5 s, and `AlertAcked` is published.

  Files: `src/ui/AnnunciatorBar.cpp`, `src/services/AlertManager.cpp`, `src/services/HapticService.cpp`.
- [X] T021 [US3] Validate quickstart Q5: STALL raised → **the user taps the band** 10× (re-raising between taps with
  `ad alert clear stall` + `raise stall`); the vibration stops ≤1 s each time (SC-003); the page under the band doesn't
  react. Then a new CAUTION flashes and vibrates again. Record in `validation.md`.

---

## Phase 6: User Story 4 - Crisp switch clicks (P2)

**Goal:** one click per page change and control activation, never queued, never cutting a warning.

**Independent Test:** quickstart Q6.

- [X] T022 [P] [US4] Page-change click: in `src/ui/PageManager.cpp` `onLoaded`, call `HapticService::click()` once per
  settled page change (swipe and button)
- [X] T023 [P] [US4] `src/ui/HapticClick.h/.cpp`: `attachClick(lv_obj_t*)` adds a `CLICKED`/`VALUE_CHANGED` callback
  → `HapticService::click()`. Apply it to the Settings buttons, switch, dropdowns, and zone/station OK/Cancel in
  `src/pages/SettingsPage.cpp`, and to REFRESH in `src/pages/WxPage.cpp`.
- [X] T024 [US4] Validate quickstart Q6:
  - 20 swipes → the user feels exactly one click each, and none with `hapticlvl 0` (SC-004);
  - **click latency (FR-010):** with `ad set loglvl 3`, compare the `[nav] nav … ` line timestamp (page settled) with
    the following `[hap] play click` timestamp over the 20 swipes. Every difference must be ≤50 ms; record max/avg;
  - during a STALL shaker, swipes don't interrupt the pattern;
  - with `stall` unacknowledged and `ad att sweep on`: FPS median ≥28, heap ≥40 KB (SC-006).

  Record in `validation.md`.

---

## Phase 7: User Story 5 - Vibration strength and quiet mode (P2)

**Goal:** Settings "ALERTS" section with Vibration and Quiet mode, kept across reboots.

**Independent Test:** quickstart Q7.

- [X] T025 [US5] Settings section "ALERTS" in `src/pages/SettingsPage.cpp`:
  - "Vibration" dropdown Off/Low/High (`hapticlvl`);
  - "Quiet mode" switch (`quiet`);
  - "Test alert" button → `AlertManager::raise(Test1, Caution, "TEST ALERT")`, cleared after 10 s (a one-shot
    `lv_timer`);
  - refreshes on `SettingsChanged`.
- [X] T026 [US5] Validate quickstart Q7:
  - low vs high (the user feels the difference);
  - quiet: CAUTION/ADVISORY silent, STALL vibrates, clicks unchanged;
  - reboot keeps the settings;
  - **blind test with the user** (SC-005: STALL vs click vs CAUTION, 3/3).

  Tune the effect ids if the user asks. Record in `validation.md`.

---

## Phase 8: Polish & Cross-Cutting

- [X] T027 [P] Amend `docs/software_requirements_specification.md`:
  - FR-HAP-002 note: PULL UP ≤5 % (haptic in S4, voice in S5), hysteresis 17 % / 7 %;
  - FR-HAP-003 note: warnings always at full strength;
  - FR-ALR-004 note: quiet mode keeps UI clicks.
- [X] T028 [P] Amend `docs/software_architecture_specification.md`:
  - the AlertManager row (`core::AlertSet/AlertTable`, snapshot, repeat), the HapticService row
    (`IBoard::haptic(seq)`, priority, policy);
  - the §5.2 events (`AlertRaised/Cleared/Acked`; `CautionChanged` retired);
  - the SettingsStore row (v5);
  - the IBoard row (+haptic).
- [X] T029 [P] Update `README.md`: the annunciator (levels, flash, tap to acknowledge), STALL / PULL UP, Vibration
  and Quiet mode, and the `ad alert` / `ad haptic` / `ad fuel sim … nochg` commands, with a link to the S4 contract
- [X] T030 Regression and portability:
  - `pio test -e native` (all suites);
  - `pio run -e twatchs3`;
  - layering grep (`SensorDRV2605`/`drv.` only in `src/board/`; no `lv_` in services; `Cautions`/`CautionBanner`
    gone);
  - the S1 regression script (PFD FPS, nav ring, IMU-fail caution now via AlertManager);
  - the S2/S3 boot sync + weather fetch still Ok.
- [ ] T031 After the PR is merged, update `docs/product_roadmap.md` per the **status tracking rule**: S4 → ✅ Done with
  the date, the PR number and the pending manual checks; M2 stays 🚧 (S5, S6 open).

---

## Dependencies & Execution Order

- **Setup (T001–T002)** → **Foundational (T003–T013)**:
  - tests T003–T006 come before T007–T010;
  - T011 comes before T012, which comes before T013.
- **US1 (T014–T017)** needs Foundational.
- **US2 (T018–T019)** needs T014 (`raise/clear`) and T012.
- **US3 (T020–T021)** needs T014 and T016.
- **US4 (T022–T024)** needs T012 only. It can run in parallel with US1–US3, except that SettingsPage/WxPage edits are
  shared with US5, so sequence those.
- **US5 (T025–T026)** needs T010, T012 and T014.
- **Polish:** after the stories. T031 runs only after the merge.

### Parallel Opportunities

- T003 ∥ T004 ∥ T005 ∥ T006 (test files).
- T008 ∥ T009 (core modules).
- T022 ∥ T023 (PageManager vs HapticClick).
- T027 ∥ T028 ∥ T029.

## Parallel Example: tests first

```text
Task: "T003 AlertSet/table test in test/test_alerts/test_main.cpp"
Task: "T004 BatteryAlarm test in test/test_battery_alarm/test_main.cpp"
Task: "T005 HapticPolicy test in test/test_haptic_policy/test_main.cpp"
Task: "T006 settings v5 test in test/test_settings/test_main.cpp"
```

## Implementation Strategy

1. **MVP = Foundational + US1 + US2 + US3.** One alert system, unmistakable low battery, acknowledge.
2. **US4 clicks:** the feel of an instrument.
3. **US5 settings:** comfort.
4. **Docs, regression, merge.** Then the roadmap update (T031).

## Notes

- The haptic feel is subjective: every "feels" check is the user's call, recorded verbatim in `validation.md`.
- **If an installed API differs** from what a task names (e.g. SensorDRV2605 method names, a GO-bit read), follow
  `.pio/libdeps/twatch_ultra/SensorLib` and note the difference in `validation.md`.
