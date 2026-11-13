---

description: "Task list for NVG Mode, Eject & Power UX (Slice S7)"
---

# Tasks: NVG Mode, Eject & Power UX (Slice S7)

**Input**: Design documents from `/specs/008-nvg-eject-power/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/serial_commands.md,
contracts/board_interface.md, quickstart.md

**Tests**: INCLUDED. Constitution Principle V and spec FR-020 require Unity tests for the new `src/core/` modules,
written before the implementation.

**Organization**:
- US1: screen timeout and raise-to-wake (P1)
- US2: Pull-to-Eject deep sleep (P1)
- US3: NVG night mode (P2)
- US4: power profile and battery-life report (P2)

## Format: `[ID] [P?] [Story] Description`

## Conventions (apply to every task)

- **Running PlatformIO:** `& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio …`.
  - Native tests need WinLibs GCC on PATH.
  - Serial tools open COM3 with **DTR/RTS low**. USB CDC drops lines, so check state with `ad screen`, `ad wake`
    and `ad rec`, not single log lines.
  - After a deep sleep the USB CDC port disappears and comes back on wake; scripts must reopen COM3.
- **Rules from S0–S6 still hold:**
  - LilyGoLib, ESP-IDF sleep and LVGL display internals only in `src/board/`;
  - `src/core/` is pure;
  - no LVGL in services;
  - only the UI task calls LVGL;
  - every PMU or expander access holds the I2C lock;
  - Snapshot writes come from one task.
- **Write helper scripts and C string edits with the Write/Edit tools** (shell heredocs mangle backslashes).
- **Never run `ad set reset` / Restore defaults on the device without asking the user** (it erases the Wi-Fi
  credentials). Never print or change the Wi-Fi credentials.
- **Wearer checks** (raises, gestures, NVG readability, dark-room alert levels): ask, and record the answer verbatim
  in `validation.md`.
- **References:** contracts in `contracts/`; entities in `data-model.md` §n; decisions in `research.md` R1–R10;
  checks in `quickstart.md` Q1–Q8.

---

## Phase 1: Setup

- [X] T001 Create the test folders `test/test_screen_policy/`, `test/test_raise_detector/`,
  `test/test_eject_gesture/` and `test/test_nvg/`
- [X] T002 Verify the baseline on branch `008-nvg-eject-power`:
  - `pio test -e native` (205/205);
  - `pio run -e twatch_ultra`;
  - `pio run -e twatchs3`.

  Create `specs/008-nvg-eject-power/validation.md` and record flash/RAM use and the day-mode PFD FPS median as the
  baseline.

---

## Phase 2: Foundational (blocking)

**Purpose:** the `IBoard` additions with an **on-device probe** of the three risky mechanisms (display sleep/wake,
our own deep-sleep sequence, the flush filter), settings v8, and the new events.

- [X] T003 Add `WakeCause` to `src/core/Types.h` (data-model §7) and the 4 methods to `src/board/IBoard.h`
  (`setDisplayOn`, `setNvgFilter`, `deepSleep(wakeMask, timerSec)`, `wakeCause`), with the doc comments from
  `contracts/board_interface.md`. Add wake-mask bit constants: Button 1, Imu 2, Timer 4.
- [X] T004 Implement the additions in `src/board/BoardUltra.cpp`.
  - **`setDisplayOn`**: under the I2C lock, `instance.sleepDisplay()` / `instance.wakeupDisplay()`; also put the
    touch controller to sleep when off and wake it when on (`instance.wakeupTouch()`), if that is reliable.
  - **`setNvgFilter`**: a flag read by a flush wrapper.
    - Install the wrapper once after LVGL is up: keep the original `flush_cb` (`lv_display_private.h`) and
      replace it with one that, when the flag is set, calls `core::NvgColor::apply()` on the buffer before
      forwarding.
    - Until T033 exists, call a stub.
  - **`deepSleep`**: the R6 sequence, from LilyGoLib's public API and without the vendor 2 s delay.
    - PMU: disable all IRQs, enable `PKEY_SHORT`, clear the status.
    - Peripherals: touch sleep, `sleepDisplay`, `uninstallSD`, expander pins low, `disablePowerMeasure`,
      `pmu.enableSleep`.
    - `powerControl(...)` off for haptic, GPS, speaker, NFC, SD and radio, and for the sensor unless the IMU wake
      bit is set.
    - Keep ALDO2 (display) and the RTC backup charging.
    - `Wire.end`, open-drain the unused pins (the vendor list).
    - Wake: ext1 on `PMU_INT` (ANY_LOW), plus the timer when timerSec > 0; then `esp_deep_sleep_start()`.
    - If a needed member is not public, add an `AERODECK-PATCH-2` to `tools/patch_lilygolib.py` (idempotent,
      ADR-008 style) and note it for ADR-014.
  - **`wakeCause`**: map `esp_sleep_get_wakeup_cause()` and `esp_sleep_get_ext1_wakeup_status()`; cache it in
    `begin()`.
- [X] T005 Implement the same methods in `src/board/BoardS3.cpp`.
  - `setDisplayOn`: backlight 0 / restore the brightness.
  - `setNvgFilter`: the shared flush wrapper.
  - `deepSleep`: returns false (not validated on S3).
  - `wakeCause`: the same mapping.
  - `pio run -e twatchs3` must compile.
- [X] T006 **Probe on the device** with a temporary `ad probe s7 <display|sleep|nvg>` command. It is removed in
  T048, or kept as a test hook if useful. Record the results in `validation.md`:
  1. **Display:** off for 5 s, then on. The content is intact, the wake time is under 300 ms, and no touch events
     arrive while off.
  2. **Sleep:** `deepSleep(Button, 0)`.
     - The time from the call to `esp_deep_sleep_start` is logged and is ≤200 ms.
     - The button wakes the watch, and `wakeCause()` = Button.
     - The RTC time is still correct after 2 minutes asleep.
     - COM3 comes back.
  3. **NVG:** the filter is on with a stub (invert, or green fill); the whole screen is affected, and the FPS
     overlay works.

  If any of the three fails, stop and revise `research.md` R1, R4 or R6 before continuing.
- [X] T007 [P] Extend `test/test_settings/test_main.cpp` for schema 8 (data-model §1):
  - `nvg` 0..1, default 0;
  - `scrto` 0..5, default 1;
  - `raise` 0..1, default 1;
  - v7 → v8 migration keeps the existing keys and adds the defaults;
  - `findSettingKey` for the 3 new names.
- [X] T008 Add `Nvg`, `ScrTo`, `Raise` to `src/core/SettingDefs.{h,cpp}`. Set `kSchemaVersion` = 8 and update the
  comment. T007 passes.
- [X] T009 [P] Add `NvgChanged`, `EjectRequested`, `ScreenChanged`, `ScreenCommand` to `src/services/EventBus.h`, with comments from
  data-model §10.

**Checkpoint:** T006 probes pass; the native tests are green; both boards build.

---

## Phase 3: User Story 1 — Screen timeout & raise-to-wake (Priority: P1) 🎯 MVP

**Goal:** the screen turns off after the idle time (default 15 s) and comes back on for a wrist raise, the button
(press consumed) or a new caution or warning.

**Independent test:** quickstart Q1 and Q2.

### Tests (write first)

- [X] T010 [P] [US1] Write `test/test_screen_policy/test_main.cpp` for `core::ScreenPolicy` (data-model §2):
  - `timeoutMs` table, including Never = 0;
  - turns off at exactly the timeout counted from the later of the last touch and the last wake;
  - touches keep it on;
  - Never never turns off;
  - `setupShown` blocks turning off;
  - the button when off: TurnOn, consumed = true, cause Button;
  - the button when on: not consumed, and restarts the idle time;
  - raise when off with raise enabled and available: TurnOn; raise disabled or unavailable: ignored; raise when on:
    ignored;
  - a Caution or Warning alert when off: TurnOn, cause Alert; an Advisory: ignored; an alert when on extends the
    timeout;
  - touch suppression after wake: `suppressTouch()` is true right after TurnOn, until a release is seen or 300 ms
    pass with no touch down (FR-002);
  - a timeout change takes effect at the next update;
  - millis wrap.
- [X] T011 [P] [US1] Write `test/test_raise_detector/test_main.cpp` for `core::RaiseDetector` (data-model §3),
  with synthetic gravity sequences at 100 Hz:
  - arm hanging (gravity along the long axis) → rotate into the face-up viewing pose over 400 ms → hold 150 ms:
    fires exactly once;
  - already face-up for 10 s: never fires;
  - away for less than 300 ms, then back: no fire;
  - a rise slower than 1 s: no fire;
  - reaching the pose but leaving before 150 ms: no fire;
  - pitch over 45° or roll over 35° in the end pose: no fire;
  - an arm swing (passing through the pose for under 100 ms): no fire;
  - two separate raises: two fires;
  - noisy samples (±0.05 g): still one fire.

### Implementation

- [X] T012 [P] [US1] Implement `src/core/ScreenPolicy.{h,cpp}` (no Arduino, LVGL or IDF). T010 passes.
- [X] T013 [P] [US1] Implement `src/core/RaiseDetector.{h,cpp}`.
  - Reuse the pitch/roll conventions of `core::Attitude` (S1).
  - Keep the thresholds as named constants in the header.
  - T011 passes.
- [X] T014 [US1] Create `src/ui/ScreenService.{h,cpp}` (UI task only).
  - `begin()`; `tick(nowMs)` every loop iteration (internally every 50 ms):
    - read `lv_display_get_inactive_time()` and the setup-overlay state. Add `bool NetSetupOverlay::isShown()` to
      `src/ui/NetSetupOverlay.{h,cpp}`; it does not exist yet;
    - call `ScreenPolicy::update`;
    - apply TurnOff/TurnOn through `board().setDisplayOn()`.
  - On turning on: `lv_obj_invalidate(lv_screen_active())`, `lv_display_trigger_activity(nullptr)`, and log
    `[scr] on (<cause>) <ms> ms` measured to the first flush.
  - `bool onButton()`: returns true if the press was consumed.
  - `onRaise()`; `onAlert(level)`; `isOn()`.
  - Configure from the settings `scrto` and `raise`, plus "raise available" (attitude status Ok).
  - Post `ScreenChanged` on every change.
  - **Touch suppression** (FR-002): while `ScreenPolicy::suppressTouch()` is true, drop LVGL pointer input (wrap
    the indev read, or `lv_indev_reset` plus ignoring presses), so a finger already on the glass never acts as a
    tap.
  - Publish `Snapshot<ScreenSnapshot>` (data-model §11).
- [X] T015 [US1] Feed the raise detector.
  - Pass each gravity sample (the S1 callback path in `src/services/AttitudeService.cpp`, UI task, O(1)) to a
    `RaiseDetector` owned by `ScreenService`. A raise → `ScreenService::onRaise()`.
  - Keep the per-sample work O(1). Do not change the S1 LOCK/attitude behaviour.
- [X] T016 [US1] Integrate in `src/app/main.cpp`:
  - `ScreenService::begin()` after the UI is ready, and `ScreenService::tick(now)` in `loop()`;
  - **skip `lv_timer_handler()` while the screen is off**; `board().loop()` and the event queue keep running;
  - `onButton`: `if (!ScreenService::onButton()) PageManager::buttonPressed();`;
  - `AlertRaised` events with level ≥ Caution **and `b = 1` (new or re-armed)** → `ScreenService::onAlert()`.
    In `src/services/AlertManager.cpp`, publish `AlertRaised` with `b = 1` for New/Rearmed and `b = 0` for
    `RaiseResult::Updated` (data-model §10, FR-004). The existing S4 subscribers ignore `b`.
  - `applySetting(ScrTo|Raise)` → reconfigure `ScreenService`.
- [X] T017 [US1] Add `ad screen`, `ad screen on|off`, `ad screen raise` (`contracts/serial_commands.md`).
  - The console runs on the svc task, so post a `ScreenCommand` event (data-model §10; add it to
    `src/services/EventBus.h`) instead of calling `ScreenService` directly.
  - `ad screen` reads `Snapshot<ScreenSnapshot>` (data-model §11), written by the UI task.
  - `ad nvg on|off` (US3, T038) lives in the same module as these commands.
- [X] T018 [US1] Add to `src/pages/SettingsPage.{h,cpp}` a **DISPLAY** section, next to Brightness:
  - "Screen timeout" −/+ cycling 10 s, 15 s, 30 s, 1 min, 5 min, Never;
  - "Raise to wake" on/off, shown as "n/a" when unavailable.

  Use the existing row and smallButton helpers; refresh on `SettingsChanged`.
- [X] T019 [US1] **On-device Q1** (screen timeout): items 1–7. Record the off timing in 10 trials, the wake
  latency, the consumed button press, the alert wake and the setup screen.
- [X] T020 [US1] **On-device Q2** (raise-to-wake, wearer):
  - 10 raises, with the hit count;
  - a 5-minute walk, with the false-wake count;
  - `raise 0` disables it.

  Tune the `RaiseDetector` constants if needed. Then re-run T011, recording the final values and the reason in
  `research.md` R5.
- [X] T021 [US1] Measure the PFD FPS median with the screen-off logic active (screen on). It must be ≥30 and no
  more than 2 below the T002 baseline.

**Checkpoint:** US1 works alone. The screen timeout is the largest battery win and can ship by itself.

---

## Phase 4: User Story 2 — Pull-to-Eject deep sleep (Priority: P1)

**Goal:** a double tap, then a pull on the striped handle, saves the data and sleeps within 2 s. The button wakes
the watch to the PFD with the recorder and settings intact.

**Independent test:** quickstart Q3 (and Q4 for the spike).

### Tests (write first)

- [X] T022 [P] [US2] Write `test/test_eject_gesture/test_main.cpp` for `core::EjectGesture` (data-model §4,
  travel 171 px):
  - **fires:** tap, tap (gap ≤400 ms), press, drag 70 px down, release;
  - pullFrac follows dy and is clamped to 1;
  - **does not fire, for each of:**
    - a single tap;
    - a drag without taps;
    - a double tap with a 450 ms gap;
    - a double tap, then a press after 3.1 s;
    - a double tap, then dragging 30 px sideways;
    - a double tap, then a pull of 60 px (35 %) and release;
    - a long press (>300 ms) as the first "tap";
    - a tap that moves 15 px;
  - after a short pull it is Idle, and a new double tap re-arms it;
  - Fired is terminal until `reset()`;
  - tick-driven disarm reported as a reason (timeout).

### Implementation

- [X] T023 [P] [US2] Implement `src/core/EjectGesture.{h,cpp}`, with a `lastCancelReason()` for the
  `[eject] disarmed (…)` log. T022 passes.
- [X] T024 [US2] Extend `src/services/PowerService.{h,cpp}` (data-model §8):
  - `addPreSleepHook(PreSleepHook)`, up to 4 hooks;
  - `eject(uint8_t source)`, run on the svc task when `EjectRequested` is received, with a 500 ms deadline for the
    service steps:
    1. log the start;
    2. `RecorderService::saveNow("eject")`;
    3. stop the AudioService queue and amp; `HapticService` stop;
    4. `NetService` abort (end the sync or portal, Wi-Fi off);
    5. call the hooks and take the smallest non-zero wake time;
    6. set `g_ejecting` and wait ≤200 ms for the UI quiesce semaphore;
    7. log the timing, flush the serial port, store the timing in an `RTC_NOINIT`/`RTC_DATA_ATTR` struct for
       `ad wake`;
    8. take the I2C lock and call `board().deepSleep(Button [| Imu if cap], timerSec)`;
    9. if it returns false: log `eject unsupported`, release, clear `g_ejecting`, and turn the screen back on
       through an event.
  - Add the stop/abort entry points to AudioService, HapticService and NetService only where they do not exist
    yet; reuse the existing ones first.
- [X] T025 [US2] UI side of the quiesce handshake, in `src/app/main.cpp` / `ScreenService`: when
  `PowerService::ejecting()` is set, stop calling `lv_timer_handler()`, call `board().setDisplayOn(false)`, give
  the semaphore, and idle (`delay(10)`) until the reset.
- [X] T026 [US2] At boot in `src/app/main.cpp`:
  - log `[boot] wake=<cause>` from `board().wakeCause()`;
  - add `ad eject` (posts `EjectRequested` with source 1; replies `OK ejecting`, or `ERR unsupported` when the
    board returns false);
  - add `ad wake` (cause, plus the eject timing from RTC memory);
  - after a button wake, clear the PMU IRQ status at boot (in `BoardUltra::begin`), so the wake press is not
    delivered again as a click (research R6).

  The commands live in `PowerService::registerCommands()`.
- [X] T027 [US2] Create `src/ui/widgets/EjectHandle.{h,cpp}`:
  - an `lv_obj` filling the top band (`S(28)` high, full width, clipped by the rounded corners);
  - diagonal yellow (#FFD000) / black stripes drawn in a `LV_EVENT_DRAW_MAIN` callback (or a 2-colour canvas
    tile), with the centred label "PULL TO EJECT";
  - the ARMED state is visible: the text becomes "ARMED — PULL", and the border flashes;
  - while pulling, the handle is translated down by `pullFrac × travel`;
  - pressed, pressing and released events go to `core::EjectGesture`, with a 100 ms LVGL timer calling `tick`;
  - clear `LV_OBJ_FLAG_GESTURE_BUBBLE` and `SCROLL_CHAIN`, so drags never change pages;
  - on Fired, post `EjectRequested` (source 0) and log `[eject] fire`;
  - log armed and disarmed.
- [X] T028 [US2] Host the handle in `src/pages/PfdPage.{h,cpp}`:
  - create the EjectHandle at the top of the PFD screen;
  - `onEnter`: `AnnunciatorBar::setTopOffset(Theme::S(28))`; `onLeave`: `setTopOffset(0)`.

  Add `setTopOffset(int32_t)` to `src/ui/AnnunciatorBar.{h,cpp}`: it re-aligns the bar to
  `LV_ALIGN_TOP_MID, 0, offset` (research R8). Check that only the PFD heading row (heading tape, MAX G, FLT NM) is covered while alerts are shown, and that it
  reappears when they clear.
- [X] T029 [US2] **On-device Q3** (eject and wake):
  - 5 full gestures: the time to sleep in each, which must be under 2 s;
  - wake by the button: the time to PFD must be ≤4 s, `ad rec` and the settings unchanged;
  - 20 wrong gestures, expecting 0 ejects (wearer);
  - eject with STALL active (annunciator below the handle);
  - eject while charging;
  - `ad eject`.

  Record everything in `validation.md`.
- [X] T030 [US2] **IMU-wake spike** (research R7, ≤2 h, quickstart Q4).
  - Behind `-DAERODECK_IMU_WAKE` in `src/board/BoardUltra.cpp`:
    - list the hub's virtual sensors (are 57 or 67 present?);
    - before sleep, arm the wake sensor, set the hub interrupt active-low, keep the sensor rail on, and add GPIO8
      to the ext1 mask;
    - map GPIO8 to `WakeCause::Imu`.
  - Measure the wake rate over 10 tilts and the sleep current with and without the flag (T042 method).
  - Promote (flag default on, `Cap::ImuWakeInterrupt`) only if ≥8/10 and the extra current is under 1 mA;
    otherwise leave it off.
- [X] T031 [US2] Write `docs/adr/014-deep-sleep-and-wake-sources.md`:
  - why the vendor `sleep()` is not used (the 2 s delay; timer XOR pins; sensor rail);
  - the sequence we use;
  - the RTC backup kept on;
  - no `RTC_DATA_ATTR` recorder mirror (the NVS save at eject);
  - the wake sources;
  - the spike result, which **closes SRS OI-1**;
  - any `AERODECK-PATCH-2`;
  - **rails at eject:** `deepSleep()` switches supplies off but is invoked only by `PowerService::eject()`; the
    lease refcounts are void after the wake reset (constitution III note);
  - the wake press is cleared at boot (it would otherwise arrive as a harmless click).

**Checkpoint:** US1 and US2 both work. FR-REC-005 "survives deep sleep" is verified.

---

## Phase 5: User Story 3 — NVG night mode (Priority: P2)

**Goal:** one toggle turns every element phosphor green on black at 10 % brightness, on all pages and live, kept
across restarts and eject. The alert levels stay distinguishable.

**Independent test:** quickstart Q5 and Q6.

### Tests (write first)

- [X] T032 [P] [US3] Write `test/test_nvg/test_main.cpp`.
  - **`core::NvgColor::toGreen565`** (data-model §5):
    - black → 0x0000; white → **0x07EC** (#00FF66 in RGB565);
    - mid grey → green ≈ 50 % (±2 steps);
    - pure red, green and blue map to their luminance (0.299 / 0.587 / 0.114 ±1 step);
    - for all 65 536 inputs: no red bits, and the output lies on the black → #00FF66 ramp (blue =
      round(green6 × 12 / 63));
    - it is monotonic along a grey ramp;
    - `apply()` on a buffer equals a per-pixel call.
  - **`core::NvgStyle::forLevel`** (data-model §6):
    - day: no inverse or outline for all levels;
    - NVG Warning: inverse, visible toggling every 250 ms;
    - NVG Caution: inverse, always visible;
    - NVG Advisory: outline, not inverse.

### Implementation

- [X] T033 [P] [US3] Implement `src/core/NvgColor.{h,cpp}` (three constexpr per-channel tables, in-place
  `apply`). T032 passes for the colour part.
- [X] T034 [P] [US3] Implement `src/core/NvgStyle.{h,cpp}`. T032 passes.
- [X] T035 [US3] Wire the flush filter in `src/board/BoardUltra.cpp` and `BoardS3.cpp`: replace the T004 stub with
  `core::NvgColor::apply(buf, w*h)`.
- [X] T036 [US3] Apply NVG in `src/app/main.cpp`:
  - `applySetting(Nvg)` → `board().setNvgFilter(on)`, re-apply the brightness, post `NvgChanged`, and log
    `[nvg] on|off`;
  - `applySetting(Bright)` → 10 % while NVG is on, otherwise the stored level (FR-016);
  - apply both in `setup()` **before the first PFD frame** (FR-014).
- [X] T037 [US3] NVG alert styling in `src/ui/AnnunciatorBar.cpp`:
  - in NVG, use `core::NvgStyle`:
    - inverse: background `Theme::hudGreen()`, text black;
    - outline: black background, green border and text;
    - blink: add a 250 ms timer when any NVG Warning is shown;
  - keep the S4 1 Hz unacknowledged flash;
  - re-render on `NvgChanged`.
- [X] T038 [US3] Add `ad nvg on|off` (sets the `nvg` setting) next to the `ad screen` commands from T017 (same
  module). Add an "NVG" on/off row to the Settings DISPLAY section in `src/pages/SettingsPage.cpp`.
- [X] T039 [US3] **On-device Q5** (wearer): all pages green, live widgets, 10 % brightness, the three alert levels
  in a dark room, brightness changes while in NVG, off restores, reboot and eject keep NVG from the first frame.
- [X] T040 [US3] **On-device Q6**: the PFD FPS median over 60 s in day mode and in NVG. It must be **≥30**
  (constitution IV) and no more than 2 below the T002 baseline. If not, apply the research R1 fallbacks in order
  and re-measure.

**Checkpoint:** US1–US3 work.

---

## Phase 6: User Story 4 — Power profile & battery-life report (Priority: P2)

**Goal:** a 1 Hz profiling log and a measured battery-life report against the ≤50 mA / ≥22 h target.

**Independent test:** quickstart Q8.

- [X] T041 [US4] Add `ad pwr log on|off` to `src/services/PowerService.cpp`.
  - Print the 1 Hz `[pwr] prof …` line from `contracts/serial_commands.md` on the svc tick.
  - Inputs: `PowerSnapshot` (%, mV, charging, VBUS, `fuelFlowPctPerH`), `ScreenSnapshot` (screen on, NVG), and
    `PowerService::refcount()` per rail, plus the Wi-Fi hook state.
  - `ma=n/a` when `battmA == kCurrentUnavailable`.
  - Not persisted.
- [X] T042 [US4] **Measurements** (quickstart Q8; research R9). Ask the user for the battery-only runs: they need
  the watch off USB for hours.
  1. Capture the profile logs per state, off USB: PFD on day; PFD on NVG; screen off idle; ejected for ≥24 h (%
     before and after).
  2. If the user has a USB meter: the battery-full VBUS current per state.
  3. Store the raw numbers in `validation.md`.
- [X] T043 [US4] Write `docs/power_report.md`:
  - the method(s) and the battery capacity source;
  - a table of state × method × average current;
  - the raise false-wake rate (T020);
  - the typical-day profile (R9);
  - the estimated battery life;
  - **the verdict against the ≤50 mA / ≥22 h target** (met or not, and by how much);
  - SC-006 (ejected ≤ 1/10 of screen-off idle);
  - the main consumers and next steps.

  Link it from the README and the SAS §10 test-strategy row.

---

## Phase 6b: Horizon tilt reference (FR-023, SRS FR-PFD-012) — added during implementation

**Goal:** at the wearer's request, the horizon reads level at a chosen face tilt (default 30°) instead of only
when the watch lies flat. The wearer decided to fold this into S7. A real compass bearing was also requested; it is
not possible (no magnetometer: I2C scan + hub firmware checked), so the top tape stays the seconds (wearer's
decision).

- [X] T051 [P] Extend `test/test_attitude/test_main.cpp`: upright reference (90°: level, bank, nose down) and a
  30° reference (level at 30°, flat reads −30°, 0° = S1). Extend `test/test_settings/test_main.cpp` with `attref`
  (U8 0..90, step 15, default 30).
- [X] T052 Implement `core::Attitude::setReferenceTiltDeg()` (rotation of the up vector about the 3–9 o'clock axis)
  in `src/core/Attitude.{h,cpp}`. Add setting `attref` to `src/core/SettingDefs.{h,cpp}`;
  `AttitudeService::setReferenceTilt()`; apply it in `src/app/main.cpp`; Settings row "Horizon tilt" −/+ 15° in
  `src/pages/SettingsPage.{h,cpp}`.
- [X] T053 On the device: measure the hub axes with labelled poses (flat / upright / 12 o'clock right / tilted
  away); wearer check at 90° and at 30° ("Feels right").

---

## Phase 7: Polish & Cross-Cutting

- [X] T044 [P] Update `docs/software_requirements_specification.md`:
  - FR-EJT-004 note (timeout steps, consumed press, touches ignored);
  - FR-NVG-003 note (inverse and 2 Hz blink, inverse, outline);
  - NFR-PWR-004 note (no current ADC; drain rate);
  - **close OI-1** with a reference to ADR-014;
  - FR-REC-005 note (deep sleep: saved at eject).
- [X] T045 [P] Update `docs/software_architecture_specification.md`:
  - §4 the `IBoard` methods;
  - §5.3 the eject sequence (hooks, quiesce, own board sequence; no `RTC_DATA_ATTR` mirror);
  - §6 the `ScreenService` row and `PowerService` eject/profiling;
  - §7.1 settings v8 and the recorder at eject;
  - §8 NVG by the flush filter;
  - the ADR table + ADR-014.
- [X] T046 [P] Update `README.md`:
  - NVG, screen timeout, raise, eject;
  - the new `ad` commands;
  - the power report link;
  - the S7 contract link.
- [X] T047 **On-device Q7**: the DISPLAY rows are present and persist across a reboot. Restore defaults is checked
  **only with the user's consent**; otherwise `test_settings` covers it, and that is noted.
- [X] T048 Remove or keep (as documented test hooks) the T006 probe commands. Final checks:
  - `pio test -e native` (all green, with the new count);
  - `pio run -e twatch_ultra`;
  - `pio run -e twatchs3`;
  - free internal heap ≥40 KB in day mode, in NVG, and with the screen off (`ad info`).

  Record the flash/RAM delta against T002.
- [X] T049 Complete `specs/008-nvg-eject-power/validation.md`: SC-001…SC-007 each with its evidence (or "pending
  manual" with the reason). Mark all tasks [X].
- [X] T050 At merge time, update `docs/product_roadmap.md` per the status tracking rule:
  - S7 → ✅ with the date, the PR number and the pending manual checks;
  - record the measured battery life against the M3 exit criterion.

---

## Dependencies & Execution Order

- **Setup (T001–T002)** → **Foundational (T003–T009)**:
  - T003 before T004 and T005;
  - T006 needs T004;
  - T007 before T008.
- **US1 (T010–T021)** needs the Foundational phase.
  - T010/T011 before T012/T013;
  - T014 needs T012;
  - T015 needs T013 and T014;
  - T016 needs T014;
  - T017 and T018 need T016;
  - T019–T021 are last.
- **US2 (T022–T031)** needs the Foundational phase and US1's T016 (the quiesce lives in the same loop code, and the
  wake button path).
  - T022 before T023;
  - T024 → T025 → T026;
  - T027 needs T023;
  - T028 needs T027;
  - T029 needs T024–T028;
  - T030 needs T029;
  - T031 needs T030.
- **US3 (T032–T040)** needs the Foundational phase (the filter hook from T004); independent of US2.
  - T032 before T033/T034;
  - T035 needs T033;
  - T036 needs T035;
  - T037 needs T034;
  - T038 needs T036;
  - T039 and T040 are last.
- **US4 (T041–T043)** needs US1–US3 (measures their states). T042 needs T041; T043 needs T042.
- **Polish (T044–T050)** after the stories. T050 at merge.

## Parallel Opportunities

- T007 ∥ T009 (different files) after T003.
- US1 tests T010 ∥ T011; implementations T012 ∥ T013.
- US2 T022 ∥ T024 (gesture core vs. PowerService); T023 ∥ T024.
- US3 T032 → T033 ∥ T034. US3 can be built in parallel with US2 once T004 exists (different files, except
  `main.cpp`: serialise T036 after T025/T026).
- Docs T044 ∥ T045 ∥ T046.

### Parallel Example: User Story 1

```text
T010 test_screen_policy  ∥  T011 test_raise_detector
T012 ScreenPolicy        ∥  T013 RaiseDetector
```

### Parallel Example: User Story 3

```text
T032 test_nvg  →  T033 NvgColor  ∥  T034 NvgStyle
```

## Implementation Strategy

- **MVP = Phase 1 + 2 + US1.** The screen timeout alone is the biggest battery improvement and is shippable.
- **Then US2** (the eject: the slice's signature, and it closes FR-REC-005 deep sleep).
- **Then US3** (NVG).
- **Then US4** (measurement; needs hours of wall time off USB, so start T042 runs as soon as US1–US3 are on the
  device, and do the docs while it drains).
- Stop and validate at each checkpoint. Wearer checks are asked for and recorded verbatim.
