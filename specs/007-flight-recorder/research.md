# Research: G-Force & Flight Recorder (Slice S6)

Checked on 2026-11-12 against:
- SensorLib 0.3.4 (`SensorBHI260AP`, `BoschSensorID.hpp`, `BoschSensorDataHelper.hpp`, `bhy2_defs.h`);
- LilyGoLib 0.2.0 (`LilyGoUltra::initSensor`: firmware `bosch_bhi260_klio.h` because the Ultra defines
  `USING_XL9555_EXPANDS`; uploaded to the BHI260AP RAM at every boot, `setBootFromFlash(false)`);
- the S1 `BoardUltra::configureAttitude` (gravity vector, 100 Hz, callbacks on the UI task in `board().loop()`);
- `SettingsStore` (restore defaults clears the whole `aerodeck` namespace, blobs included) and `SerialConsole`
  (`ad reboot` → `esp_restart()`).

## R1 · Step source
- **Finding:**
  - The BHI260AP offers a **step counter** virtual sensor (`BHY2_SENSOR_ID_STC` = 52, `SensorBHI260AP::STEP_COUNTER`).
    SensorLib parses it as a `uint32` (`bhy2_parse_step_counter`). It reports on change (sample rate 1 Hz is enough).
  - Whether the KLIO firmware image includes it can't be read from the headers: `configure()` returns false and logs
    "not present" when a virtual sensor is missing.
  - The firmware is uploaded to RAM at every ESP32 boot, so **the hardware count starts at 0 at each boot**.
- **Finding (boot order):** the BHI260AP is initialised in the **deferred probe** (`probeDeferred` →
  `instance.initSensor()`, ≈3.4 s after boot), after which `main.cpp` calls `AttitudeService::onImuReady()`. Any
  configuration before that fails.
- **Decision:**
  - Board: `configureMotion()` enables STC at 1 Hz with a callback that caches the latest count; RecorderService
    polls `board().stepCount(out)` at 1 Hz (the SAS §4 sketch already has `stepCount()`).
  - **`RecorderService::onImuReady(bool imuOk)`** is called from `deferredProbeTask` next to
    `AttitudeService::onImuReady`; it calls `configureMotion(50)`, sets the step/accel availability and resets the
    `StepAccumulator` baseline (the sensor firmware was just uploaded, so its count starts at 0 there).
    `RecorderService::begin()` only loads the saved data and registers the commands.
  - Pure `core::StepAccumulator`: baseline 0 at boot; `delta = raw − last` when raw ≥ last; when raw < last the sensor
    restarted → `delta = raw`; a delta larger than 6 steps/s × elapsed s + 20 is rejected as implausible (logged).
  - **Verify first on the device** (tasks): STC present, it counts while walking, and the count is 0 after
    `ad reboot`.
- **Fallback (if STC is not in the KLIO image):** the step detector (STD = 50, one event per step) counted in the
  callback. If neither exists: steps show "--" on the Ultra, and the slice records the gap in validation.md and the
  roadmap. A software pedometer is out of scope (constitution VIII).
- **Alternatives:** our own pedometer on accelerometer data (more code, worse accuracy, more power); polling
  `bhy2_get_virt_sensor_data` (not needed, events work).

## R2 · Max G source
- **Finding:** the accelerometer virtual sensors (`ACC` = 4 corrected, `ACC_PASS` = 1) include gravity;
  `getScaling(id)` gives LSB→m/s² or LSB→g depending on the firmware; `configureRange(id, g)` sets the range; the
  S1 code notes the absolute gravity scale was never confirmed.
- **Decision:**
  - Board enables `ACC` (corrected) at **50 Hz** (FR-REC-002 "≥50 Hz") in `configureMotion()`. The callback computes
    the magnitude `|a|` in g, keeps the running maximum and the latest value (floats under a tiny spinlock);
    `takePeakG(peak, last, rangeG)` returns the peak since the previous call (and resets it), the latest magnitude
    (for `g_now`) and the active range. RecorderService polls it at 1 Hz → today's Max G with the local time of that poll (minute
    resolution, FR-004).
  - **Scale:** use `getScaling(ACC)` as reported, then check on the device that the magnitude at rest is
    1.00 ± 0.05 g (`ad rec` shows the live magnitude). If it reads ≈9.8 instead, the scale is in m/s² and the board
    divides by 9.80665. Record the finding in validation.md (it also settles the S1 note about the gravity scale).
  - **Range:** request ±16 g with `configureRange(ACC, 16)` if accepted, otherwise keep the default. The saturating
    value is shown as "<range>+" (spec edge case).
- **Measured (T012):** `configureRange(ACC, 16)` is accepted (`getConfigure().range` = 16) but `getScaling()` keeps
  reporting 1/4096 (the ±8 g step): at rest |a| read 0.50. The board now uses `range / 32768` (= 1/2048 g) → 1.00 g at
  rest. The hub also reports a ≈2 g transient right after the accelerometer starts, so peaks in the first 2 s are
  discarded (`kAccelSettleMs`). The step counter (STC 52) is present in the KLIO image (no fallback needed).
- **Rationale:** the corrected accelerometer is calibrated by the hub; 50 Hz is the required minimum and keeps the
  UI-task FIFO work small (≈11 bytes/sample).
- **Alternatives:** linear acceleration (gravity removed): not what FR-REC-002 asks; 100 Hz: more FIFO traffic on the
  UI task for no visible benefit at minute resolution.

## R3 · Where the motion callbacks run
- **Finding:** SensorLib delivers events from `instance.sensor.update()` inside `board().loop()` on the **UI task**
  (ADR-010); S1 already measured this path for the 100 Hz gravity vector.
- **Decision:** the step and accel callbacks only do O(1) work (store the count; `sqrtf` + max). No LVGL, no services
  are called from them. RecorderService (svc task) polls the board at 1 Hz. FPS is measured with the recorder active
  (SC-006).

## R4 · Persistence
- **Finding:** `SettingsStore::restoreDefaults()` clears the whole `aerodeck` namespace incl. blobs (would erase the
  history, violating FR-015). SAS §7.1 already plans "NVS `rec` (hourly) + `RTC_DATA_ATTR` (on sleep)".
- **Decision:**
  - Own NVS namespace **`rec`**, one blob `day` (format v1, CRC-16; 100 bytes: today + 7 history records; NM is not
    stored, it is derived from steps). Written by RecorderService on the svc task.
  - **When:** every **10 minutes if changed** (stricter than FR-009's hour: loss ≤10 min), at each rollover,
    and on `esp_restart()` through **`esp_register_shutdown_handler`** (covers `ad reboot` and any future planned
    restart path). `saveNow()` is public for S7's eject (which also adds `RTC_DATA_ATTR`).
  - **Wear:** ≤144 writes/day × ~4 NVS entries (32 B) → each 4 KB page of the 20 KB NVS partition is erased about
    once a day → far beyond the device lifetime at 100 k cycles.
  - **Corrupt / unknown version** → start empty, log once, raise nothing (not safety-relevant).
- **Rationale:** survives settings restore; tiny; no new partition.
- **Alternatives:** the settings namespace blob (erased by restore defaults); FAT file (wear-levelled but power-loss
  fragile, ADR-009 notes); RTC RAM only (lost on power loss).

## R5 · Day boundaries (pure `core::DayLog`)
- Local day number `localDay = floor((utc + tzOffsetMin·60) / 86400)`.
- State: `today` (day number or `kUndated` = −1 while the clock was never valid), `history[7]` newest first.
- `advance(clockValid, localDay)`:
  - clock invalid → nothing;
  - `today == kUndated` → `today.day = localDay` (FR-007, counted values belong to the current date);
  - `localDay > today.day` → push today into history, then `min(localDay − today.day − 1, 7)` empty days, start a
    new today (FR-006/FR-008);
  - `localDay ≤ today.day` → nothing (backward clock or time zone change, FR-007).
- Evaluated at 1 Hz in the svc tick → reset within 1 s of midnight (FR-006 "within 2 s").
- **Test hook** `ad rec rollover`: calls `advance(true, today.day + 1)`.

## R6 · Flight Miles math
- NM = steps × stride_cm / 185 200 (exact integer math until display; `double` for display).
- Display: `<10` → 2 decimals, `≥10` → 1 decimal, "--" when the step source is unavailable (FR-003).
- History NM is recomputed from steps with the current stride (FR-002).

## R7 · Stride setting (schema v7)
- `stride` U8, centimetres, 30..150, default 75, editable. Settings → RECORDER row with −/+ (5 cm per tap, like
  Brightness); the console accepts any cm value in range.

## R8 · Recorder page and chart
- `PageId::PlaceholderB` becomes `PageId::Recorder`; `RecorderPage` replaces the placeholder in the ring.
- Layout: title; big NM (`dseg7` font as the PFD) + "NM"; rows STEPS, MAX G (+ time); a 7-bar chart built from 7
  plain `lv_obj` bars (tappable, no `lv_chart` needed): today highlighted green, others cyan, empty days as an
  outline; weekday letters below; the selected day's values in one line above the chart. Sizes via `Theme::S()`
  inside the rounded-corner safe area.
- Refresh from `RecorderSnapshot` at ≤2 Hz while visible (seq check).

## R9 · PFD slots
- `PfdPage` reads `RecorderSnapshot` in its existing tick: MAX G "2.7" (one decimal), FLT NM "3.42"/"12.4"/"--".
  The corner labels stay where S3's curvature fix put them.

## R10 · T-Watch S3
- `BoardS3::configureMotion` returns false (the S3's BMA423 step counter is not wired in this project; compile-only
  target) → "--" on S3. Noted in the plan; no `#ifdef` outside `src/board/`.
