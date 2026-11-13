# Research: NVG Mode, Eject & Power UX (Slice S7)

**Date**: 2026-11-12 · **Spec**: [spec.md](./spec.md) · **Plan**: [plan.md](./plan.md)

Sources read for this research:
- LilyGoLib 0.2.0 `LilyGoWatchUltra.{h,cpp}` (`sleep()`, `lightSleep()`, `sleepDisplay()`, `checkWakeupPins()`);
- `LV_Helper_v9.cpp` (display flush, buffers);
- the T-Watch Ultra variant `pins_arduino.h`;
- SensorLib `bhy2_defs.h`;
- the AeroDeck sources (`Theme`, `AnnunciatorBar`, `PfdLayout`, `BoardUltra`, `main.cpp`).

---

## R1 — NVG: how to turn every element green without touching every page

**Decision:** recolour pixels on their way to the panel, plus a few explicit NVG rules.
- **Flush filter:** while NVG is on, a filter in `src/board/` maps every RGB565 pixel to its luminance, rendered as
  a phosphor-green shade (0 → black, max → #00FF66). It wraps the LVGL display flush callback, the same way
  `BoardUltra` already wraps the touch read.
  - The mapping is a pure function, `core::NvgColor::toGreen565(px)`, built from three small per-channel lookup
    tables (32 + 64 + 32 entries), plus a 64-entry blue table that keeps the output on the black → #00FF66 ramp
    (white → 0x07EC, data-model §5). That is about 7 operations per pixel and no large table.
  - It is unit-tested on the host.
  - LilyGoLib uses `LV_DISPLAY_RENDER_MODE_PARTIAL` on the Ultra with two DMA buffers that are re-rendered every
    frame, so recolouring the buffer in place before `pushColors` is safe.
  - **Correction (implementation):** LilyGoLib **does** define `_SWAP_COLORS` for the Ultra (and the S3 on IDF 4.x),
    so its flush byte-swaps RGB565. The board's own flush does the same; the NVG path writes swapped pixels
    directly.
  - Reading the original `flush_cb` needs LVGL's private display struct, so the board replicates LilyGoLib's
    flush with public calls instead.
  - **Measured (validation T006):** the first filter version cost 30 → 23 fps. Byte tables and 4 px per iteration
    with `-O3` reached 27. The **asynchronous flush** (recolour/swap + the blocking QSPI `pushColors` on a core-0
    task; LVGL renders into the second buffer meanwhile) gives **35 fps in day mode and NVG**.
- **Explicit rules:** the only places that need more than a colour change are the alert levels (FR-015), handled
  in `AnnunciatorBar` from the pure `core::NvgStyle` rules (R2), and the brightness (R3).
- **Why:**
  - It guarantees FR-014 ("no element falls back to its day colour"), including live widgets, custom-drawn horizon
    pixels (ADR-011), fonts and the eject handle, with zero per-page code.
  - It applies within one frame, with no page rebuild.
  - About 59 `Theme::` colour calls and 3 hard-coded colours stay untouched.
- **Cost:**
  - Partial buffers are 1/6 of the screen (about 34 k px).
  - The PFD redraws about 60–80 k px per frame at ~30 fps. At ~6 ops per pixel that is roughly 2–3 % of a 240 MHz
    core.
  - Measured in Q6 (SC-007): the median must be **≥30 fps** (constitution IV) and no more than 2 below the T002
    baseline.
  - Fallbacks, in order, if it is not met:
    1. process two pixels per 32-bit word, with the tables in internal RAM (IRAM/DRAM, not flash);
    2. run the filter on the DMA-idle side of the double buffer;
    3. shrink the ADI per ADR-011.
  - LVGL already flushes only the regions it invalidates, so "filter only the dirty areas" is no saving.
- **Alternatives considered:**
  - *An NVG palette in `Theme` plus rebuilding all page screens:* every page must reset its caches; there is a risk
    of stale pointers; and custom-drawn pixels (the horizon) still need NVG code.
  - *Converting all colours to shared `lv_style_t` objects (SAS §8 wording):* a large refactor of 4 pages and 3
    widgets for one feature (VIII).
  - *An LVGL style colour filter:* it is not inherited by children and covers only the object's own drawing.
  - *A 64 K-entry lookup table:* 128 KB of internal RAM breaks the ≥40 KB heap rule (IV).
- **SAS §8 note:** "Theme swaps every palette token" becomes "the board flush filter recolours the frame; Theme
  exposes `nvg()` for the few explicit rules". `NvgChanged` is still sent (SAS §5 event list).

## R2 — NVG alert styling (FR-015)

**Decision:**
- Pure `core::NvgStyle::forLevel(level, nvg, flashPhase)` returns {inverse, blinkVisible}.
  - Day mode: today's colours (unchanged).
  - NVG WARNING: inverse (green background, black text) blinking at 2 Hz (250 ms on / 250 ms off). This is faster
    than the 1 Hz unacknowledged flash, so it stays distinct from CAUTION.
  - NVG CAUTION: inverse and steady (the existing 1 Hz flash still applies while unacknowledged).
  - NVG ADVISORY: green outline on black.
- `AnnunciatorBar` applies the rules.
- The flush filter turns a "green" background into full-intensity green and black text into black, so the result
  is the intended inverse.
- **Rationale:** with colour gone, the level is told apart by shape and motion: outline, solid, or blinking solid.
  Host-testable.

## R3 — NVG brightness (FR-013, FR-016)

**Decision:**
- In NVG, `applyBrightness()` calls `setBrightnessPct(10)`.
- When NVG is off, it uses the stored `bright` setting (20–100 %).
- The `bright` setting can still be changed in NVG: it is stored, but the screen stays at 10 % (FR-016).
- On the Ultra, 10 % maps to AMOLED level 25/255.
- If 10 % turns out to be unreadable, or the panel's floor is higher, the measured level is noted in the power
  report (spec assumption).

## R4 — Screen off and wake while running (US1)

**Decision:**
- **Off:**
  - `IBoard::setDisplayOn(false)` calls `instance.sleepDisplay()`: AMOLED sleep-in, and `DISP_EN` low through the
    expander. The display rail (ALDO2) stays on, as LilyGoLib advises.
  - The UI task stops calling `lv_timer_handler()`, so there is no rendering and no touch read; touches are
    ignored (FR-002).
  - `board().loop()` keeps running: button events and gravity samples are still needed.
  - Services are unaffected (FR-003).
- **On:**
  - `setDisplayOn(true)` calls `wakeupDisplay()`.
  - The current screen is invalidated, LVGL activity is triggered, and `lv_timer_handler()` resumes. The first
    frame shows current values, because the pages read snapshots on tick.
  - The AMOLED sleep-out takes about 120 ms and a frame about 35 ms, so this is well under 1 s.
- **Idle time:**
  - Pure `core::ScreenPolicy` decides on/off from now, the last input time, the timeout index, raise events, the
    button, alerts raised, and whether the setup screen is shown.
  - The last input is LVGL's `lv_display_get_inactive_time()`, plus the button.
  - The button press that wakes the screen is consumed (FR-002).
- **Rationale:** it uses vendor-supported calls; no light sleep of the CPU (out of scope; the report measures it).
- **Alternative:** `lightSleep()` from LilyGoLib turns off haptics, GPS and speaker and needs the touch panel as a
  wake source. That breaks background services (chime, alerts) and FR-003.

## R5 — Raise-to-wake (FR-002)

**Decision:**
- Pure `core::RaiseDetector`, fed with the gravity samples the watch already streams at 100 Hz (ADR-010, UI task).
- A raise is detected when **all** of these hold:
  1. the wrist was **not** in the viewing pose for at least 300 ms, for example arm hanging or watch face vertical;
  2. within 1 s it moves into the viewing pose, with the face up and tilted toward the eyes: gravity along −z ≥
     0.7 g, and |pitch| ≤ 45°, |roll| ≤ 35°;
  3. it holds the pose for 150 ms.
- Thresholds are constants tuned on the device (Q2), and the tests use recorded-style sequences.
- **Rationale:**
  - It is deterministic, host-testable, and uses no new sensor configuration.
  - The BHI260AP "wrist tilt gesture" (virtual sensor 67) may not be in the KLIO firmware image LilyGoLib loads, and
    cannot be unit-tested.
- **Alternative (kept as a fallback, not built):** the hub's wrist-tilt sensor, if Q2 shows the detector misses
  more than 1 in 10 raises.
- **Result (T020):** with the hub healthy after wake (see R6), the wearer reported 9–10 of 10 raises. The device
  counted 10 raise detections and 8 wakes; the other 2 raises came while the screen was still on. The constants
  were kept as planned. An earlier run scored ≤ 6/10 because the gravity data was frozen after a deep-sleep wake
  (R6 finding).
- **Duty:** no extra cost. The hub already runs gravity for the attitude display. While the screen is off, the
  gravity rate stays 100 Hz: halving it would save little next to the panel, and S1's LOCK logic keeps its timing.

## R6 — Deep sleep (eject) sequence and the 2 s budget (FR-008)

**Finding:**
- `LilyGoUltra::sleep()` contains a **2 s delay** (`while (i--) delay(500)` with i = 4) before
  `esp_deep_sleep_start()`. This alone breaks FR-008's 2 s budget.
- It also enables **either** the timer **or** the pins as a wake source, never both. S8 needs both: button plus
  timer.
- It turns off `POWER_SENSOR`, which rules out IMU wake (R7).

**Decision:**
- `BoardUltra::deepSleep(wakeMask, timerSec)` runs its own sequence, built from LilyGoLib's public API and the same
  steps, without the delay:
  1. PMU IRQs: disable all, enable only `PKEY_SHORT`, then clear the status.
  2. Touch sleep, display sleep and `end`, `uninstallSD`.
  3. Expander pins low: `DISP_RST`, `DRV_EN`, `DISP_EN`.
  4. `disablePowerMeasure`, `pmu.enableSleep`.
  5. Power off: haptic, GPS, speaker, NFC, SD, radio; and the sensor, unless IMU wake is enabled (R7).
  6. Keep the display rail on: LilyGoLib measured about +600 µA if it is cut.
  7. Keep the RTC backup battery charging, so the clock survives (unlike the vendor default, which disables it).
  8. `Wire.end`, open-drain the unused pins.
  9. `esp_sleep_enable_ext1_wakeup(1<<PMU_INT, ANY_LOW)`, plus `esp_sleep_enable_timer_wakeup` when timerSec > 0.
  10. `esp_deep_sleep_start()`.
- If a step is not public in LilyGoLib 0.2.0, it is done with an `AERODECK-PATCH-2` in `tools/patch_lilygolib.py`
  (ADR-008 mechanism). This is decided while implementing; a patch is noted in ADR-014.
- **Orchestration** (`PowerService::eject`, svc task):
  1. Log, then save the recorder: `RecorderService::saveNow("eject")`, ≤20 ms (S6 measured).
  2. Stop the audio queue and amp, haptics and the network (abandon sync or portal).
  3. Run the registered **pre-sleep hooks**: an empty list in S7; S8's timer registers one that returns a wake time
     (FR-009).
  4. Ask the UI task to quiesce: a flag and a semaphore, ≤200 ms. The UI stops LVGL and shows nothing.
  5. Hold the I2C lock and call `board().deepSleep()`.
  - Budget: the service steps get ≤500 ms of wall time, enforced by checking the deadline; anything late is skipped
    and logged. The board sequence takes about 50 ms without the delay. Target: under 1 s end to end, measured in Q3.
- **Wake:** deep sleep restarts the chip. `IBoard::wakeCause()` maps `esp_sleep_get_wakeup_cause()` and the ext1
  status to Button, Imu, Timer, PowerOn or Other.
  - After a button wake, the PMU interrupt status is **cleared at boot**, so the wake press is not delivered again
    as a click. If it were, it would only go to the PFD, which is shown anyway; this is noted in ADR-014.
  - The wake cause is logged at boot (`boot: wake=button`) and shown by
  `ad wake`. BootGuard is unaffected: a wake boot reaches "stable" like any other.
- **State across sleep:** the recorder is restored from NVS `rec` (saved in step 1), and settings from NVS. An
  `RTC_DATA_ATTR` mirror is **not** added (SAS §7.1 listed it): the eject save makes it redundant, and
  RTC memory is lost on power-off anyway (VIII). SAS §7.1 is updated.
- **Finding (implementation):** cutting the sensor hub rail in deep sleep (as LilyGoLib does) leaves the BHI260AP
  delivering **frozen** gravity/accelerometer values after the wake boot, until a normal reboot. **Decision:** keep
  the hub powered and stop its virtual sensors before sleeping. The whole ejected watch stays below 1 mA (USB
  meter).
- **Steps during sleep:** the hub firmware is re-uploaded on boot, so its count restarts. Steps
  walked while ejected are not counted (spec edge case); S6's `StepAccumulator` already handles the restart.

## R7 — Wake by wrist movement from deep sleep (SRS OI-1, FR-010)

**Finding:**
- On the Ultra, the BHI260AP interrupt goes to `SENSOR_INT` = **GPIO8**, an RTC-capable GPIO (0–21 on the S3). So
  ext1 wake is possible **in principle**.
- It requires that, during sleep:
  - the sensor rail stays on (the vendor `sleep()` cuts it);
  - a wake-up virtual sensor is armed, either "wake gesture" (57), "wrist tilt" (67) or any-motion, if the loaded
    firmware has it;
  - the hub's interrupt is set active-low, so GPIO8 can share ext1 ANY_LOW with `PMU_INT`.
- Cost: the hub's current during sleep, likely several hundred µA.

**Decision:**
- A **time-boxed spike** (≤2 h, Principle VIII) behind the compile flag `AERODECK_IMU_WAKE` (default **off**).
- Spike steps:
  1. List the hub's virtual sensors at runtime; check that 57 or 67 is present.
  2. Arm it before sleep, keep the sensor rail on, and add GPIO8 to the ext1 mask.
  3. Measure whether a wrist tilt wakes the watch in 10 tries, and the sleep current with and without.
- **Promote** (flag default on, `Cap::ImuWakeInterrupt` set) only if it wakes in ≥8 of 10 tries **and** adds less
  than 1 mA in sleep. Otherwise **button-only wake** (the SRS OI-1 fallback).
- Either way, ADR-014 records the result and closes OI-1.
- **Result (T030):** the firmware offers 67, 57 and 55, but neither the wrist tilt (67) nor significant motion (55)
  woke the chip. **Not promoted:** button-only wake (ADR-014).
- **Alternative:** a periodic timer wake to poll the IMU. It boots fully every few seconds (about 3.5 s at 80 mA),
  which is far worse than staying awake. Rejected.

## R8 — Pull-to-Eject handle placement and gesture (FR-006, FR-007)

**Placement:**
- The handle fills the **top band** of the watch face: the S0 banner zone, `bannerH = S(28)` = 48 px on the Ultra,
  which is ≥48 px, the NFR-USE-001 touch target. It is drawn with diagonal yellow/black stripes and the text
  "PULL TO EJECT".
- **Conflict:** the S4 annunciator bar uses the same band on every page and stays visible while any alert is
  active, even acknowledged. A low-battery STALL, exactly when eject is wanted, would hide the handle.
- **Decision:** on the watch face only, `AnnunciatorBar::setTopOffset(px)` moves the bar down one band, so it covers
  the heading-tape row (S(24) + margin) while an alert is shown. The handle stays at the top. Other pages are
  unchanged.
  - The heading tape is the least critical PFD element when an alert demands attention.
  - **The whole heading row is covered:** the heading tape plus **MAX G and FLT NM** (S6), which sit beside it.
    They come back when the alerts clear, and they stay visible on the Recorder page (stated in the spec
    assumption).
  - Both remain ≥48 px targets.
- **Alternatives:**
  - *A smaller tab overlapping the bar:* the touch target is under 48 px, and overlapping tap zones (ack vs. arm)
    are ambiguous.
  - *Making room by shifting the PFD down by one more band:* on the Ultra the bottom ZULU/LOCAL row would hit the
    rounded corners (computed: about 33 px inset needed at 8 px from the edge, only 11 px available).
  - *The bottom of the screen:* contradicts FR-EJT-001 ("at the top").
- The spec's assumption is updated with this.

**Gesture** — pure `core::EjectGesture`, fed with {press, move(dy), release, tick} and timestamps:
- States: Idle → (tap) FirstTap → (second tap within 400 ms) **Armed** (shown for ≤3 s) → (press and drag down)
  Pulling(dy) → release with dy ≥ 40 % of travel → **Fire**.
- A "tap" is a press and release within 300 ms that moves less than 10 px.
- Travel = S(100) (171 px on the Ultra), so 40 % ≈ 68 px. The handle visibly follows the finger downwards.
- Disarm on: 3 s timeout, releasing short, horizontal movement > 20 px, or a pause between the taps > 400 ms (back
  to FirstTap or Idle).
- The handle object removes `GESTURE_BUBBLE` and `SCROLL_CHAIN`, so its drags never change pages.

## R9 — Power measurement method (FR-018, FR-019, SC-005, SC-006)

**Finding:** the AXP2101 has **no battery current ADC** (`readPmu` returns `kCurrentUnavailable`, S0). Current
cannot be read on the watch.

**Decision:** use two methods; the report states which one gave each number.
1. **Fuel-gauge discharge** (always available):
   - Off USB, hold one state for ≥2 h. The profiling log records % and mV each second.
   - Average current ≈ Δ% × capacity ÷ Δt. The T-Watch Ultra's battery capacity is taken from LilyGoLib/LilyGo docs
     and stated in the report.
   - S1's `FuelFlow` estimator (%/h) is logged alongside.
   - Resolution is 1 % of capacity, so: watch face on and screen off need ≥2 h; deep sleep needs ≥24 h, and a drop
     of ≤1 % in 24 h is reported as an upper bound.
2. **USB power meter, battery full** (if the user has one): with charging terminated, the system runs from VBUS. The
   meter's 5 V current × 5 V ÷ 3.85 V gives a battery-equivalent figure. It is quick per state, but sub-mA deep
   sleep is below most meters' resolution.

- **The profiling line** (`ad pwr log on`), 1 Hz:
  `pwr t=… pct=… mv=… chg=… vbus=… flow=…%/h scr=on|off nvg=0|1 rails=lora:0,amp:0,gnss:0,nfc:0,wifi:0 ma=n/a`.
  The `ma` field is kept, so a board with a current ADC (S3: also AXP2101 → n/a) can fill it.
- **The report** `docs/power_report.md` contains:
  - a table of states × method × average;
  - the typical-day profile: 16 h worn, of which 2 h screen-on in total (about 120 glances of 15 s plus page use);
    0.5 h in NVG; the rest screen-off; 8 h ejected overnight; radios off; hourly chime outside quiet hours;
  - the estimate: capacity ÷ the weighted average current;
  - a verdict against the ≤50 mA / ≥22 h target (NFR-PWR-002, redefined from ≥48 h in S7), with the main consumers and next steps if it is not met.
- **Spec amendment:** FR-018 says "battery current"; amended to "battery current where the hardware measures it,
  otherwise the battery voltage, level and estimated drain rate".

## R10 — Settings schema v8

**Decision:** add three keys.
- `nvg`: Bool, default 0.
- `scrto`: U8 index 0..5 into {10, 15, 30, 60, 300, Never}, default 1 (15 s).
- `raise`: Bool, default 1.

All are editable. Restore defaults resets them (FR-022). Migration v7 → v8 adds the defaults.

The setting changes reach the UI through the existing `SettingsChanged` event:
- `nvg` turns the flush filter on or off, re-applies the brightness and sends `NvgChanged`;
- `scrto` and `raise` update `ScreenPolicy`.
