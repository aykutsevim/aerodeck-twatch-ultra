# Data Model: PFD Watch Face (S1)

## 1. AttitudeSnapshot (`Snapshot<AttitudeSnapshot>`, written on the UI task by the BHI callback)

| Field | Type | Notes |
| :--- | :--- | :--- |
| `pitchDeg` | float | + = nose up. Display clamps to ±30° |
| `rollDeg` | float | + = right wing down (wrist rolled right). −180…180 |
| `level` | bool | LOCK state (hysteresis, §5) |
| `status` | enum AttStatus {Align, Ok, Fail} | Align while the IMU probe is Pending; Fail if IMU Failed/SimulatedFail or no sample for >1 s |
| `sampleMs` | uint32 | millis() at callback (latency, research R9) |
| `seq` | uint32 | increments per sample |

**Derivation (`core::Attitude`, host-tested)** from gravity g = (x, y, z) in the remapped sensor frame:
```
(ax, ay, az) = axisMap(g)                 // constant table, verified on device (spike)
roll  = atan2(ax, az)                     // degrees
pitch = atan2(-ay, sqrt(ax² + az²))       // degrees, −90…90
if |pitch| > 80°: roll is held at its last value (avoids spin near vertical)
EMA α = 0.5 on pitch/roll (wrap-aware for roll)
```

## 2. LockDetector (`core::LockDetector`, host-tested)

State: `locked` (bool), `inBandSinceMs`.
```
inBand  = |pitch| ≤ 3 && |roll| ≤ 3
outBand = |pitch| > 5 || |roll| > 5
!locked && inBand for ≥ 500 ms  → locked
 locked && outBand              → !locked (immediately)
status != Ok                    → !locked
```

## 3. ClockSnapshot (`Snapshot<ClockSnapshot>`, written by TimeService at 1 Hz and on change)

| Field | Type | Notes |
| :--- | :--- | :--- |
| `valid` | bool | RTC integrity guaranteed AND year ≥ 2025 |
| `utc` | time_t | Seconds since epoch |
| `tzOffsetMin` | int16 | From setting `tzoff` |

Per frame, the PFD derives local/UTC fields from `gettimeofday()` (ms resolution) plus `tzOffsetMin`. It
uses `valid` from the snapshot.

**Pure helpers (`core::ClockMath`, host-tested)**: `splitLocal(utc, ms, tzOffsetMin) → {h, m, s, headingDeg (float, s·6 + ms·0.006), dayRollover}`, `formatHHMM`, `parseIso8601Utc("2026-11-11T14:37:20Z") → time_t | error`, `isPlausible(year)`.

## 4. FuelState & FuelFlow (`core::FuelFlow`, `core::FuelState`, host-tested)

| Field | Type | Notes |
| :--- | :--- | :--- |
| `samples[30]` | {uint32 tMin; uint8 pct} | Ring, 1/min while discharging |
| `ratePctPerH` | float or NaN | NaN = "--" (span < 10 min) |
| `lowFuel` | bool | <15 % and !charging → true; >17 % or charging → false |

Reset on a charging-state change or a jump >5 % between consecutive samples.

## 5. EicasSnapshot (part of PowerSnapshot, extended)

`battPct`, `charging`, `vbus`, `dieTempC10`, `valid` (from S0) + `fuelFlowPctPerH` (float, NaN = unavailable) + `lowFuel`.

## 6. Settings (schema v2)

| Key | Type | Default | Range | Step | Editable |
| :--- | :--- | :--- | :--- | :--- | :---: |
| `tzoff` | I32 | 180 | −720…840 | 30 | yes (Settings page "Local offset") |

Migration 1→2: add `tzoff` = default. Existing keys unchanged.

## 7. New cautions

| CautionId | Text | Raised | Cleared |
| :--- | :--- | :--- | :--- |
| `ClockNotSet` | CLOCK NOT SET | Boot with an invalid clock | `ad time set` succeeds |
| `LowFuel` | LOW FUEL | FuelState.lowFuel | Hysteresis clear |

## 8. New events

`ClockChanged` (time set via serial or a tzoff change), `AttitudeStatusChanged` (Align→Ok/Fail).

## 9. PFD layout model (Ultra 410×502 portrait; computed by `core::PfdLayout`, scale = 410/240 ≈ 1.71)

The T-Watch Ultra runs in **portrait**: LilyGoLib's default rotation 0 gives LVGL a 410-wide × 502-tall display
(`DISP_WIDTH 502 / DISP_HEIGHT 410` in the variant describe the panel before rotation). Boxes as computed (x, y, w, h):

| Element | Box (px) | Redraw cadence |
| :--- | :--- | :--- |
| MAX G / FLIGHT NM | (11, 48, 68, 41) / (331, 48, 68, 41) | static "--" |
| Heading tape | (86, 48, 238, 41) | 1° steps (≈6/s, ADR-011) |
| Hour tape (left) | (11, 96, 68, 238) | on hour change |
| ADI | (86, 96, 238, 238), capped for FPS (ADR-011) | on attitude change (≥0.2°) |
| Minute tape (right) | (331, 96, 68, 238) | on minute change |
| EICAS strip | (11, 341, 388, 62) (FUEL QTY · FLOW · EGT · CHG) | 1 Hz |
| ZULU / LOCAL row | (11, 410, 190, 58) / (209, 410, 190, 58) | on minute change |
| Flags | ATT ALIGN / ATT FAIL over the ADI | on status change |

The content starts at y = 48 so the S0 caution banner (48 px) never covers it. The screen's corners are rounded
with **r = 64 px**: `PfdLayout::compute(w, h, cornerR)` keeps every box inside the visible area (host-tested with
`insideRounded`), and the outer top boxes are pushed down if a corner arc would clip them
(`minTopForRounded`). The diagnostics overlay sits at the bottom centre for the same reason. Landscape and square screens
(e.g. the S3's 240×240) use the original side-by-side arrangement.
