# ADR-010: Attitude from the BHI260AP gravity virtual sensor, delivered on the UI task

- **Status:** Accepted (slice S1, 2026-11-11)
- **Amends:** AD-SAS-001 §5.1 (planned a dedicated 100 Hz attitude task on core 1 with an ESP32-side
  complementary filter)

## Context
The PFD needs pitch and roll at ≥30 FPS with ≤50 ms motion-to-display latency (NFR-PERF-001/002). On the
T-Watch Ultra the IMU is a Bosch BHI260AP smart sensor. LilyGoLib uploads Bosch's standard
`app30_shuttle_bhi260` firmware (~123 KB, ~2 s; deferred out of the boot path in S0). That firmware runs on-chip
accelerometer+gyroscope fusion and exposes virtual sensors, including the **gravity vector** (id 28).
LilyGoLib already routes the BHI interrupt to its event loop, and `LilyGoUltra::loop()` drains the FIFO with
`sensor.update()`.

## Decision
- Configure `GRAVITY_VECTOR` at **100 Hz, 0 ms latency** through `IBoard::configureAttitude()`. This is called
  when the deferred probe reports the IMU Ok. Until then the PFD shows **ATT ALIGN**; on failure, **ATT FAIL**.
- The SensorLib parse callback runs **on the UI task** inside `IBoard::loop()` (I2C lock held). It does only the
  math (`core::Attitude`: gravity → pitch/roll, EMA α=0.5, vertical guard; `core::LockDetector`: ±3°/0.5 s in,
  ±5° out) and writes `Snapshot<AttitudeSnapshot>`. No separate attitude task.
- Axis mapping: identity, after LilyGoLib's `TOP_LAYER_BOTTOM_RIGHT_CORNER` remap. Flat on the table reads
  g ≈ (0.00, −0.03, 1.00). The roll/pitch **signs** are confirmed by the hands-on check T014 (see
  `specs/002-pfd-watch-face/validation.md`); the mapping is a one-line constant (`kAxes` in `AttitudeService.cpp`).

## Consequences
- Measured on the device: **100–101 Hz** sample rate. Sample-to-flush latency typically **24–28 ms, max 39–44 ms**
  with continuous motion (≤50 ms ✅).
- No extra task or stack. The fusion quality is Bosch's (gyro-stabilised), not a hand-tuned complementary filter.
- The callback shares the I2C lock with touch and PMU reads. It must stay short (no logging, no LVGL).
- S6 (peak g) must confirm the absolute gravity scale. S1 uses only axis ratios (falls back to 1/4096 g/LSB if
  `getScaling()` is unavailable).
- T-Watch S3 (BMA423, no fusion): `configureAttitude()` returns false → ATT FAIL. An accelerometer fallback can be
  added later through the same `core::Attitude`, which accepts any gravity-like vector.

## Alternatives
- Game rotation vector (quaternion → Euler): more math for the same pitch/roll.
- Orientation (Euler) virtual sensor: heading conventions vary; less direct.
- Raw accelerometer + ESP32 complementary filter on a core-1 task (SAS plan): more CPU, noisier, more code.
