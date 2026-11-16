---

description: "Task list for Drop-Tank Chronograph (Slice S8)"
---

# Tasks: Drop-Tank Chronograph (Slice S8)

**Input**: Design documents from `/specs/009-drop-tank-chrono/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/serial_commands.md, quickstart.md

**Tests**: INCLUDED. Constitution Principle V and spec FR-014 require Unity tests for the new `src/core/` modules,
written before the implementation.

**Organization**:
- US1: countdown with a draining tank (P1)
- US2: BINGO FUEL at zero (P1)
- US3: keeps counting through page changes, screen off, eject and restart (P1)
- US4: Pomodoro (P2)

## Format: `[ID] [P?] [Story] Description`

## Conventions (apply to every task)

- **Running PlatformIO:** `& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio …`.
  - Native tests need WinLibs GCC on PATH.
  - Serial tools open COM3 with **DTR/RTS low**. USB CDC drops lines, so cross-check with `ad chrono`, `ad wake`
    and `ad boot`.
  - Deep sleep removes COM3; scripts reopen it (scratchpad `serlog.py`, `adser.py`).
- **Rules from S0–S7 still hold:**
  - SensorLib / LilyGoLib only in `src/board/`;
  - pure `src/core/`;
  - no LVGL in services;
  - display power changes only on the UI task;
  - one writer per Snapshot.
- **Write helper scripts and C string edits with the Write/Edit tools** (shell heredocs mangle escapes).
- **Never run `ad set reset` on the device without asking.** Never print or change the Wi-Fi credentials.
- **Wearer checks:** ask, and record the answer verbatim in `validation.md`.
- **References:** `data-model.md` §n, `research.md` R1–R7, `quickstart.md` Q0–Q7, `contracts/serial_commands.md`.

---

## Phase 1: Setup

- [X] T001 Create `test/test_chrono_model/` and `test/test_chrono_blob/`. Verify the baseline on branch
  `009-drop-tank-chrono`:
  - `pio test -e native` (251/251);
  - both builds.

  Create `specs/009-drop-tank-chrono/validation.md` with the baseline (RAM, flash, PFD fps).

---

## Phase 2: Foundational (blocking)

**Purpose:** prove the RTC-alarm wake, then add the pure model, the blob, the alerts and settings v9.

- [X] T002 **RTC-alarm wake** in `src/board/BoardUltra.cpp` (research R2, data-model §7).
  - In `deepSleep()`, when `kWakeTimer` is set with `timerSec > 0` and the RTC is Ok: read the RTC, compute
    now + `timerSec` in UTC, then:
    - `instance.rtc.resetAlarm()`;
    - `setAlarm(hour, minute, second, day, PCF85063_NO_ALARM)`;
    - `enableAlarm()`;
    - ext1 mask |= `1ULL << RTC_INT`.
  - Remove `RTC_INT` from the open-drain pin list in that case, and **enable its RTC pull-up**
    (`rtc_gpio_pullup_en`, `rtc_gpio_pulldown_dis`) because the PCF85063 INT is open-drain (U2).
  - The alarm is relative to the RTC's current reading, so it is used even when the integrity flag is false (time
    never set) (U3).
  - Without the RTC (`periph(Rtc) != Ok`): `esp_sleep_enable_timer_wakeup` (and log it).
  - `readWakeCause()`: ext1 with the `RTC_INT` bit → `WakeCause::Timer`.
  - In `begin()` after the vendor init, under the I2C lock: `rtc.resetAlarm()` + `disableAlarm()`.
- [X] T003 Add the test hook `ad probe s8 alarm <s>` to `src/services/BoardCommands.cpp`, which calls
  `board().deepSleep(kWakeTimer, s)` (the panel must already be off: `ad screen off` first).
  - **On-device Q0:** a 60 s and a 300 s alarm wake on their own at ±1 s against the PC clock, and `ad wake` shows
    `wake=timer`.
  - Also check that there is **no false instant wake** (the pull-up works): the sleep lasts the full time.
  - Record the results in `validation.md`.
  - If the alarm path fails, stop and revise R2 (fallback: the ESP timer with a lead margin).
- [X] T003b In `src/services/TimeService.cpp` `setSystemTime` at boot (from the RTC): set `tv_usec = 500000`, so
  the system clock is centred within the RTC's whole second (research R1, U1). An explicit time set (NTP, console)
  keeps its own sub-second value. Log `[time] system clock from RTC (+0.5 s)`.
- [X] T004 [P] Write `test/test_chrono_model/test_main.cpp` for `core::ChronoModel` (data-model §2):
  - start → remaining = interval;
  - tick before the target: no event;
  - tick at the target: Expired once;
  - `fraction` 1.0 → 0.5 → 0;
  - pause/resume keeps the remaining time exact over a 30 s pause;
  - reset → Idle, full;
  - start from Expired clears `late`;
  - **Pomodoro:**
    - work → WorkDone (cycles 1) → break → BreakDone → work;
    - the end times chain (`end += next`) with no drift when ticks are late by 400 ms;
  - `nextEventUtcMs`;
  - `restore`:
    - running in the future → unchanged;
    - countdown past → Expired + late + one event;
    - Pomodoro several phases past → the right phase and cycles, late, one event;
  - `start` while Paused = resume (the remaining time is kept) (I2);
    - paused → unchanged;
    - end − now > interval (clock reset) → clamped;
  - a clock correction of +3 s shifts the remaining time by 3 s only.
- [X] T005 [P] Write `test/test_chrono_blob/test_main.cpp` for `core::ChronoBlob` (data-model §3):
  - round trip of every field (38 B);
  - rejects a short buffer, nullptr, an unknown version or a bit flip;
  - the output is untouched on failure.
- [X] T006 [P] Extend `test/test_settings/test_main.cpp` for schema 9:
  - `chrlen` 1..120 default 25;
  - `chrmode` default 0;
  - `chrwork` 1..120 default 25;
  - `chrbrk` 5..15 step 5 default 5;
  - migration v8 → v9.

  Extend the alert tests (`test/test_alerts/` or the existing core alert test):
  - ids 42 `bingofuel` and 43 `breakover`;
  - Caution level;
  - clips `BingoFuel` and `Chime`;
  - `alertIdFromName`.
- [X] T007 Implement `src/core/ChronoModel.{h,cpp}` (pure, int64 UTC ms). T004 passes.
- [X] T008 [P] Implement `src/core/ChronoBlob.{h,cpp}`, reusing the CRC-16/CCITT-FALSE from `core::RecBlob`
  (expose it or share a helper). T005 passes.
- [X] T009 [P] Add `BingoFuel` = 42 and `BreakOver` = 43 to `src/core/Alerts.{h,cpp}` (table rows, names, texts).
  Add `chrlen`, `chrmode`, `chrwork`, `chrbrk` to `src/core/SettingDefs.{h,cpp}` with `kSchemaVersion` = 9. T006
  passes.

**Checkpoint:** the RTC-alarm wake is proven, the core is green, and both boards build.

---

## Phase 3: User Story 1 — Countdown with a draining tank (P1) 🎯 MVP

**Goal:** a timer page with presets, custom time, start/pause/resume/reset, a tank gauge and MM:SS.

**Independent test:** quickstart Q1.

- [X] T010 [US1] Create `src/services/ChronoService.{h,cpp}` (svc task, research R7):
  - `begin()`: load the blob from NVS `chrono`, `restore(now)`, register the commands;
  - `tick(now)` every svc tick (50 ms):
    - `model.tick(nowUtcMs)` from `gettimeofday`;
    - handle events;
    - publish `ChronoSnapshot` (data-model §4) at 5 Hz or on change;
  - thread-safe commands posted from the UI or console through a small queue: `start`, `pause`, `resume`,
    `reset`, `setLength(min)`, `setMode(m)`, `startTest(sec)`, `startPomoTest(workSec, breakSec)`;
  - save the blob **only** on state changes; when Idle, Paused or Expired, `tick()` does nothing but compare
    (C1);
  - read the settings `chrlen/chrmode/chrwork/chrbrk` for the config;
  - logging per `contracts/serial_commands.md`.
- [X] T011 [US1] Add the console commands `ad chrono`, `ad chrono start [s]`, `pause`, `resume`, `reset`,
  `pomo <w> <b>` in `ChronoService::registerCommands()`. Raise `CommandParser::kMaxCommands` if needed (72; count
  the registrations).
- [X] T012 [P] [US1] Create `src/ui/widgets/TankWidget.{h,cpp}`: a custom-drawn drop tank (rounded body, nose cone,
  tail fins, outline) with the fill clipped to `fraction`.
  - Colours: Theme HUD green; caution below 10 %; warning when empty.
  - `set(fraction)` invalidates only when the 1 % step changes.
- [X] T013 [US1] Create `src/pages/ChronoPage.{h,cpp}` and add `PageId::Chrono` before Settings in
  `src/ui/Page.h`. The page has:
  - the tank;
  - MM:SS in DSEG7 40 (as S6 RecorderPage);
  - the phase label;
  - preset buttons 5/15/25/50 and custom −/+ (1 min steps, 1..120), locked while running or paused;
  - Start/Pause (one toggle) and Reset, all ≥ 48 px, `noSwipe`, HapticClick.

  It refreshes from the snapshot in `onTick`. Register it in `src/app/main.cpp` between RecorderPage and
  SettingsPage. Add `ChronoService::begin` and tick there.
- [X] T014 [US1] **On-device Q1:** presets, start, a half tank at the midpoint, 5 pause/resume trials (±1 s), reset,
  custom range, locks. Record the results.

---

## Phase 4: User Story 2 — BINGO FUEL at zero (P1)

**Goal:** a caution with vibration and voice within 1 s; acknowledge stops it, and reset or start clears it.

**Independent test:** quickstart Q2 cases 1–2.

- [X] T015 [US2] In `src/services/ChronoService.cpp`:
  - on `Event::Expired`: `AlertManager::raise(AlertId::BingoFuel)`, with the text "BINGO FUEL (LATE)" when
    `late`;
  - log `[chrono] expired … err <ms>`;
  - on start or reset: `AlertManager::clear(BingoFuel)` and `clear(BreakOver)`.
  - The page shows "BINGO" and an empty tank in Expired.
- [X] T016 [US2] **On-device Q2 cases 1–2** with a script (`chrono_expiry.py` in the scratchpad):
  1. `ad chrono start 60`;
  2. wait;
  3. parse the `[chrono] expired` line and `ad alert`;
  4. compare against the PC clock.

  3 runs on the CHRONO page and 3 on another page. The wearer confirms the vibration and voice; acknowledge stops
  them; quiet mode is respected.

---

## Phase 5: User Story 3 — Continuity: screen off, eject, restart (P1)

**Goal:** it alerts on time with the screen off and when ejected (it wakes itself), and survives restarts.

**Independent test:** quickstart Q2 cases 3–4, Q3, Q4.

- [X] T017 [US3] In `src/services/PowerService.{h,cpp}`: add `constexpr uint32_t kPostponeSleep = UINT32_MAX`.
  - In `eject()`: the hooks run **before** the UI quiesce. If any hook returns it, log `eject postponed: timer
    expiring` and **return** before setting `s_ejecting`. Nothing needs undoing; the screen stays on (I1).
  - Update the S7 data-model note in `specs/008-nvg-eject-power/data-model.md` §8 with a reference line only.
- [X] T018 [US3] Register the Chrono pre-sleep hook in `ChronoService::begin` (data-model §6): not running → 0;
  r ≤ 6 s → `kPostponeSleep`; else `r − 5` s (minimum 1). Log `[chrono] sleep hook: …`.
- [X] T019 [US3] **On-device Q2 cases 3–4:**
  - screen off (timeout 10 s): the alert wakes the screen on time;
  - **ejected** right after `ad chrono start 60`: it wakes by itself and alerts at 60 ± 1 s (`ad wake` = timer,
    `[chrono] expired err`).

  3 runs each.
- [X] T020 [US3] **On-device Q3 + Q4:**
  - eject with ≤ 6 s left → postponed, then the alert;
  - eject then button-wake mid-timer → correct remaining time;
  - paused → eject → wake: still paused;
  - `ad reboot` mid-timer: remaining correct ±1 s;
  - the late case via a button-only sleep (`ad probe s7 sleep`) past the target: "BINGO FUEL (LATE)" at boot;
  - **write count (C2):** with a 5-minute timer running, the `[chrono] saved` count stays at 1 (the start) over
    60 s; pause/resume adds exactly 1 each. A crash is covered by the restore tests (T004), because the state
    saved at the last change is what a crash leaves behind.

---

## Phase 6: User Story 4 — Pomodoro (P2)

**Goal:** work and break cycles alternate automatically, with BINGO FUEL / BREAK OVER at each transition.

**Independent test:** quickstart Q5.

- [X] T021 [US4] In `ChronoService`:
  - `WorkDone` → clear BreakOver, raise BingoFuel;
  - `BreakDone` → clear BingoFuel, raise BreakOver;
  - log the cycles;
  - the hook uses the next transition.

  `ChronoPage`:
  - a mode switch (Countdown | Pomodoro, locked while running);
  - WORK/BREAK phase labels;
  - "CYCLE n";
  - in Pomodoro the presets set the work length, and a break selector 5/10/15 sets `chrbrk`.
- [X] T022 [US4] **On-device Q5:** `ad chrono pomo 60 60` through work → break → work → break, with the alerts and
  cycle counts, including one transition while ejected; reset ends the session.

---

## Phase 7: Polish & Cross-Cutting

- [X] T023 [P] Update `docs/software_requirements_specification.md`:
  - FR-CHR-001 note (custom 1–120);
  - FR-CHR-004 note (RTC-alarm wake, 5 s lead, eject postponed in the last 6 s, late at boot);
  - FR-CHR-005 note (25/5, break 5/10/15, BREAK OVER chime).
- [X] T024 [P] Update `docs/software_architecture_specification.md`:
  - §5.3 timer wake via the RTC alarm and the postpone sentinel;
  - §6 `ChronoService` row;
  - §7.1 NVS `chrono` + settings v9;
  - §8 the CHRONO page in the ring.

  Add a short "S8 addendum" to `docs/adr/014-deep-sleep-and-wake-sources.md` (timer wake = PCF85063 alarm on
  GPIO1).
- [X] T025 [P] Update `README.md`: the CHRONO page, the page ring, the `ad chrono` commands, the S8 contract link.
- [X] T026 **On-device Q6 + Q7:**
  - wearer usability, day and NVG (set/start/pause/reset < 10 s, readable; record the answer verbatim, A1);
  - PFD fps median ≥ 30 with a timer running;
  - heap ≥ 40 KB;
  - **idle drain (FR-015, C1):** screen-off current on the USB meter with the timer idle and with it running
    differs by ≤ 1 mA (the meter's resolution).
- [X] T027 Final checks:
  - `pio test -e native` all green;
  - both builds;
  - flash/RAM delta vs T001.

  Complete `validation.md` (SC-001…SC-006 with evidence). Mark all tasks [X].
- [X] T028 At merge, update `docs/product_roadmap.md` per the status tracking rule:
  - S8 → ✅ with the date, the PR number and the pending manual checks;
  - **M3 → ✅** if its definition of done holds (battery measured and met in S7, night use, timer reliable).

---

## Dependencies & Execution Order

- **T001** → **Foundational T002–T009**:
  - T002 → T003 (the probe gates everything that depends on the eject wake);
  - T003b is independent (TimeService);
  - T004/T005/T006 before T007/T008/T009.
- **US1 (T010–T014)** needs T007, T008 and T009. Then T010 → T011; T012 ∥ T010; T013 needs T010 and T012; T014
  last.
- **US2 (T015–T016)** needs US1's service.
- **US3 (T017–T020)** needs US2 (alerts) and T002/T003 (alarm wake).
- **US4 (T021–T022)** needs US2 (alerts) and T018 (the hook uses the next event).
- **Polish (T023–T028)** after the stories. T028 at merge.

## Parallel Opportunities

- T004 ∥ T005 ∥ T006 (tests); T008 ∥ T009 after T007's header exists.
- T012 (widget) ∥ T010/T011 (service).
- Docs T023 ∥ T024 ∥ T025.

### Parallel Example: Foundational

```text
T004 test_chrono_model  ∥  T005 test_chrono_blob  ∥  T006 settings/alerts tests
T007 ChronoModel → T008 ChronoBlob ∥ T009 Alerts + SettingDefs
```

## Implementation Strategy

- **MVP = Phases 1–3 (US1):** a working countdown page.
- Then US2 (the alert), then US3 (continuity through eject and restart: the roadmap's exit criterion), then US4
  (Pomodoro).
- The RTC-alarm probe (T003) runs **first**, because US3 depends on it.
- Stop and validate at each checkpoint; ask for wearer checks and record them verbatim.
