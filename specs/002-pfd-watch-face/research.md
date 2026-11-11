# Research: PFD Watch Face (S1)

Verified against the installed sources on 2026-11-11: SensorLib 0.3.4, LilyGoLib 0.2.0, LVGL 9.6.0,
and S0 on-device measurements (`specs/001-platform-foundation/validation.md`).

---

## R1. Attitude source (FR-002, FR-003, SC-002, SC-003)

**Decision**: Use the BHI260AP **gravity vector virtual sensor** (`BHY2_SENSOR_ID_GRA`, id 28), configured
at 100 Hz with 0 ms report latency. Register it with `sensor.onResultEvent(BHY2_SENSOR_ID_GRA, cb)` and
`sensor.configure(BHY2_SENSOR_ID_GRA, 100, 0)`. Pitch and roll are computed from the gravity vector in pure core code
(`core::Attitude`), followed by a light EMA (α≈0.5) only to suppress quantisation jitter.

**Rationale**:
- The firmware LilyGoLib uploads (`bosch_app30_shuttle_bhi260`, the ~123 KB image seen in S0) runs Bosch's
  accelerometer+gyroscope fusion on-chip. The gravity output is already gyro-stabilised, so no
  complementary filter on the ESP32 is needed.
- Pitch and roll from gravity are well-defined, with no magnetometer needed (the Ultra has none).
- `BoschSensorDataHelper` parses GRA like ACC (3 axes), so the existing SensorLib helpers apply.

**Delivery path**: LilyGoLib's `initSensor()` attaches the BHI INT pin to `HW_IRQ_SENSOR`.
`LilyGoUltra::loop()` (called by `IBoard::loop()` on the UI task, under the I2C lock since S0) calls
`sensor.update()`, which drains the FIFO and runs our callback **on the UI task**. The callback writes
`Snapshot<AttitudeSnapshot>`, and the PFD reads it in its per-frame timer.
→ **No separate 100 Hz attitude task** (SAS §5.1 planned one on core 1). Recorded as **ADR-010**.

**Axis mapping**: LilyGoLib sets `setRemapAxes(TOP_LAYER_BOTTOM_RIGHT_CORNER)`. The sign and axis
assignment for the portrait screen (which axis is roll and which is pitch, and their signs) **must be
verified on the device** in the first implementation task (spike T-ATT) with the watch at known angles. The mapping
is a small constant table in `core::Attitude`, so a wrong sign is a one-line fix.

**Alternatives considered**:
- Game rotation vector (quaternion → Euler): more math and the same information for pitch/roll.
- Orientation (Euler) sensor: includes heading, but conventions vary and it is less direct.
- Raw accelerometer + ESP32 complementary filter (SAS plan): more CPU and noisier. Kept as a fallback if GRA is
  unavailable (`core::Attitude` accepts any gravity-like vector).

**Deferred probe interaction (S0)**: the IMU is initialised ~3 s after Home appears. The PFD shows an
**"ATT ALIGN"** flag (aviation term for attitude-reference alignment) while the IMU is `Pending`, and
**"ATT FAIL"** if it ends `Failed`/`SimulatedFail`. The gravity sensor is configured when the deferred
probe reports the IMU `Ok` (new `IBoard::configureAttitude()` call from the probe task, under the I2C lock).

---

## R2. Horizon rendering at ≥30 FPS (FR-020, SC-001)

**Context**: S0 measured full-screen (410×502) transitions at 15–28 FPS with LilyGoLib's DMA partial
buffers (two internal 1/6-screen buffers), and 5–7 FPS with full-frame PSRAM buffers. A full-screen redraw every
frame cannot reach 30 FPS.

**Decision**:
1. A **custom-drawn ADI widget**: one plain `lv_obj` with an `LV_EVENT_DRAW_MAIN` handler that draws:
   - the sky rect (bg colour of the object);
   - the ground polygon as **two `lv_draw_triangle` calls**, clipped to the ADI area;
   - pitch ladder lines (`lv_draw_line`, every 5°, labelled every 10°);
   - the fixed aircraft symbol and roll index.
   No `transform_rotation` on large layers: LVGL 9 renders transformed objects through an intermediate layer,
   which is too expensive here.
2. **Redraw only what changes**:
   - the ADI object is invalidated only when attitude changed by ≥0.2° or the lock state changed;
   - the heading tape is invalidated each frame (small strip);
   - hour/minute tapes and the corner times only when the value changes;
   - the EICAS at ≤1 Hz.
   Typical per-frame dirty area: ADI (~280×280 = 78 k px, ~38 % of the screen) + heading strip
   (~250×40 = 10 k px), so about 43 % of a full frame.
3. Keep S0's 16 ms display refresh period (≤60 FPS cap) and DMA partial buffers.
4. **Spike first (T-PERF)**: build the ADI widget alone and measure FPS with continuous wrist motion through the
   overlay. **Gate: ≥30 FPS sustained.** If missed, apply in order:
   (a) shrink the ADI to 240×240;
   (b) disable anti-aliasing for the ladder lines;
   (c) round attitude to 0.5° to reduce invalidations;
   (d) as a last resort, extend `tools/patch_lilygolib.py` to enlarge the DMA buffers (1/6 → 1/3 screen), which
   widens the vendor patch and needs an ADR amendment.

**Rationale**: Partial invalidation in proportion to the changed area is LVGL's main lever. Triangles and lines are
cheap in the SW renderer compared with layer transforms.

**Alternatives considered**: `lv_canvas` in PSRAM with a manual rasteriser (PSRAM writes were the slow path
in S0); rotating an image (needs a transform layer, slow); a full-screen ADI (too large).

---

## R3. Tape widgets (FR-006, FR-007)

**Decision**: `TapeWidget`, a clipping container holding a column (or row, for heading) of pre-created tick
labels. Scrolling is done by moving the inner strip (`lv_obj_set_y/x`), with a fixed value box overlaid.
- Hour tape: shows ±2 neighbours (e.g. 12 13 [14] 15 16). It moves only when the hour changes (no smooth
  scroll needed).
- Minute tape: the same, per minute.
- Heading tape: smooth. Its position is computed every frame from `ms-within-second`
  (offset = (sec·6 + ms·0.006)°). The strip holds tick marks every 5° and labels every 30° (N, 03, 06, E…
  compass style is optional; numeric is required). The box shows the 3-digit heading (sec·6).

**Rationale**: Moving a small strip invalidates only the tape area. No relayout is needed per frame.

---

## R4. Clock source and validity (FR-008–FR-011)

**Findings**: `instance.rtc` is `SensorPCF85063` with `getDateTime()`/`setDateTime(RTC_DateTime)` and
`isClockIntegrityGuaranteed()` (false after an oscillator stop, i.e. power loss). The RTC is initialised in
`begin()` (it is not deferred).

**Decision**:
- At boot, `TimeService` (new, minimal) reads the RTC through `IBoard::readRtc(tm&, bool& integrity)` and
  sets the system clock with `settimeofday`. The RTC keeps **UTC**.
- **Valid** = integrity guaranteed AND year ≥ 2025. If invalid: `ClockSnapshot.valid=false`, all readouts show
  "--", and the caution `CLOCK NOT SET` is raised.
- Local time = UTC + `tzoff` minutes (new setting; −720…+840, step 30, default +180). No DST.
- Serial `ad time set <ISO-8601 UTC>` writes the RTC and the system clock, clears the caution, and publishes
  `ClockChanged`.
- S2 will replace the fixed offset with a POSIX TZ string (FR-TIM-003) and add NTP.

**Alternatives considered**: storing local time in the RTC (breaks Zulu and S2's NTP); a `tzoff` in hours (India
+5:30 needs half-hours).

---

## R5. FUEL FLOW estimate (FR-012, FR-013)

**Decision**: `core::FuelFlow`. It samples (t, battPct) once per minute while not charging, keeping a ring of 30
samples (30 min). Drain rate = −slope × 60 (%/h) by least squares over all samples ≥10 min old relative to the
newest. Output "unavailable" if the span is <10 min. History is reset when charging starts or stops, and on
a jump >5 % between samples (gauge recalibration).

**Rationale**: The AXP2101 has no battery-current ADC (S0 finding). Percent is integer-quantised, so
regression over ≥10 min gives a usable rate (resolution ≈ ±3 %/h at 10 min, ≈ ±1 %/h at 30 min).

**Alternatives considered**: voltage-based drain (non-linear and load-dependent); a fixed "mA" estimate from
rail states (would invent data). Rejected.

**SRS impact**: FR-PFD-007 "current draw (mA)" → "drain rate (%/h)". Amended in this branch.

---

## R6. HUD digit font (FR-008, FR-019)

**Decision**: Generate an LVGL font from **DSEG7 Classic Bold** (SIL OFL 1.1, keshikan/DSEG) with
`lv_font_conv` (Node is installed). Use a digits-only subset (`0-9 : - .` and space) at 2 sizes (≈40 px for the tape
boxes, ≈28 px for the corners). Commit the generated `.c` files under `src/ui/fonts/` together with the OFL
licence text and the exact generation command (`tools/gen_fonts.md`).

**Fallback**: Montserrat 40/28 (already enabled in LilyGoLib's `lv_conf.h`) if generation is blocked. The spec
requires high contrast and legibility, not a specific typeface.

---

## R7. Low-fuel and charging annunciation (FR-014, FR-015)

**Decision**:
- `core::FuelState` hysteresis: LOW when pct < 15 and not charging; clears at pct > 17 or when charging (host-tested).
- A `FuelMonitor` tick in `PowerService` (svc task, 1 Hz) raises/clears `Cautions` `LOW FUEL`.
- The charging annunciator ("CHG") is part of the EICAS widget, driven by `PowerSnapshot.charging`, polled at 1 Hz
  (≤2 s requirement met).

---

## R8. Settings schema v2 (FR-009)

**Decision**: Add `tzoff` (I32, def 180, min −720, max 840, **step 30**) and bump `kSchemaVersion` to 2. The
`SettingDef` gains a `step` field (0 = any), and `validateSetting` enforces `(v − min) % step == 0`. A stored schema 1
**migrates** (S0's state machine) by just adding the default. The Settings page gets a "Local offset" row
with −/+ buttons showing `UTC+03:00`.

---

## R9. Latency measurement (SC-002)

**Decision**: Every attitude sample carries `sampleMs` (time of the BHI callback). On each
`LV_EVENT_RENDER_READY` where the ADI was redrawn, log or track `now − sampleMs` of the attitude used. `ad att`
reports the last value and the max over the last 10 s. This measures sensor-callback-to-flush-complete latency.
BHI internal latency at 100 Hz (≤10 ms) is added in reporting.

---

## R10. Behaviour when the PFD is hidden (FR-021)

**Decision**: The PFD's 30–60 Hz `lv_timer` is created in `onEnter` and deleted in `onExit`. The gravity sensor
keeps running (S6 will need it for peak-g), but no drawing happens. On `onEnter` all widgets are refreshed
from the current snapshots before the first frame.
