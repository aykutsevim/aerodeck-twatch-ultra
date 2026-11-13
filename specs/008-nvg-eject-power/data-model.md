# Data Model: NVG Mode, Eject & Power UX (Slice S7)

All `core::` types are pure (no Arduino/ESP-IDF/LVGL) and host-tested.

## 1. Settings v8 (`core::SettingDefs`, NVS `aerodeck`)

| Key | Type | Default | Range | Meaning |
| :-- | :-- | :-- | :-- | :-- |
| `nvg` | Bool | 0 | 0..1 | NVG mode |
| `scrto` | U8 | 1 | 0..5 | Screen timeout index into `{10, 15, 30, 60, 300, Never}` s |
| `raise` | Bool | 1 | 0..1 | Raise-to-wake |
| `attref` | U8 | 30 | 0..90, step 15 | Horizon tilt: the face tilt (degrees from the ground) at which the horizon reads level (FR-023) |

- `kSchemaVersion` = 8.
- Migration v7 → v8 adds the defaults; existing keys are kept (this includes the Wi-Fi credentials).
- Restore defaults gives NVG off, 15 s, raise on, horizon tilt 30° (FR-022).
- `core::Attitude::setReferenceTiltDeg(t)` rotates the up vector about the 3–9 o'clock axis before S1's flat
  formulas.
- Measured hub frame: up = +z when flat, -y when upright (12 o'clock up), +x when 12 o'clock points right.
- Tests: `test_attitude` (upright, 30°, 0°).

## 2. `core::ScreenPolicy`

Holds the display-power state; the UI task calls it about every 50 ms.

```text
Config  { uint8_t timeoutIdx; bool raiseEnabled; bool raiseAvailable; }
Inputs  (per call) nowMs, inactiveMs (time since last touch), setupShown
Events  onButton(nowMs) -> bool consumed
        onRaise(nowMs)
        onAlert(level, nowMs)          // Caution or Warning only
State   on (bool), lastWakeMs, lastCause {Boot, Button, Raise, Alert, Touch}
Output  update(...) -> Action {None, TurnOff, TurnOn}
```

**Rules:**
- `timeoutMs(idx)`: 10000, 15000, 30000, 60000, 300000, or 0 for Never.
- **Turning off:** when on, and Never is not selected, and `setupShown` is false, and
  `min(inactiveMs, now − lastWakeMs) ≥ timeout` → TurnOff.
  - The idle time counts from the later of the last touch and the last wake. A wake gives a full timeout even when
    there was no touch.
- **Button:**
  - when off: turn on, cause = Button, and the press is consumed (`true`);
  - when on: `lastWakeMs = now` (the press counts as input), and the press is not consumed.
- **Raise:** when off, and `raiseEnabled && raiseAvailable` → TurnOn, cause = Raise. When on, it is ignored.
- **Alert** (Caution or Warning, **new or re-armed only**): when off → TurnOn, cause = Alert. When on →
  `lastWakeMs = now`, which extends the timeout.
  - An update to an alert already shown (`RaiseResult::Updated`) is never passed to `onAlert` (FR-004).
  - The `AlertRaised` event gets `b = 1` for New/Rearmed and `b = 0` for Updated (AlertManager change); `main.cpp`
    forwards only `b = 1`.
- **Touch suppression after wake:** after any TurnOn, `suppressTouch()` stays true until the first touch release
  is seen, or for 300 ms if no touch is down. The UI drops LVGL input during that window, so a finger already on the
  glass does not act as a tap (FR-002).
- **Setting changes** take effect at the next `update`.

## 3. `core::RaiseDetector`

Fed with gravity samples (x, y, z in g, sensor frame after LilyGoLib's remap) and ms.

```text
Params   viewZ = -0.70 g (face up), maxPitchDeg = 45, maxRollDeg = 35,
         awayMinMs = 300, riseWindowMs = 1000, holdMs = 150
State    Away(sinceMs) -> Rising(startMs) -> Holding(sinceMs) -> (event) -> InView
Output   feed(sample) -> bool raised (true once per raise)
```

- **Viewing pose:** `z ≤ viewZ`, |pitch| ≤ 45°, |roll| ≤ 35°, with pitch and roll from S1's `core::Attitude`
  conventions.
- A raise fires only after a period **out** of the viewing pose of at least `awayMinMs`. Holding the watch still
  face-up never re-fires.
- **Moving into the pose:** the pose must be reached within `riseWindowMs` of leaving the Away condition, and held
  for `holdMs`.
- The axis signs are confirmed on the device in Q2; the thresholds are constants in one header.

## 4. `core::EjectGesture`

```text
Params  tapMaxMs = 300, tapSlopPx = 10, doubleTapGapMs = 400, armedMs = 3000,
        travelPx (S(100)), fireFrac = 0.40, sideCancelPx = 20
Inputs  press(x,y,ms), move(x,y,ms), release(x,y,ms), tick(ms)
State   Idle | FirstTap(releasedMs) | Armed(sinceMs) | Pulling(startY, dy) | Fired
Output  state(), pullFrac() (0..1, for the handle animation), fired() (true once)
```

**Transitions:**
- Idle → tap → FirstTap.
- FirstTap → a second tap whose press is within `doubleTapGapMs` of the first release → **Armed**. If the gap is
  longer, the second tap becomes a new FirstTap.
- Armed:
  - press → Pulling;
  - a tick past `armedMs` → Idle.
- Pulling:
  - move: dy = max(0, y − startY), and pullFrac = min(1, dy/travel);
  - a horizontal movement over `sideCancelPx` → Idle;
  - release with dy ≥ fireFrac × travel → **Fired**;
  - release short of that → Idle.
- **Not a tap:** a press longer than `tapMaxMs`, or one that moves more than `tapSlopPx`, cancels FirstTap and
  Armed → Idle.
- Fired is terminal until `reset()`.

## 5. `core::NvgColor`

```text
uint16_t toGreen565(uint16_t rgb565)
```

- `lum6 = HI[high byte] + LO[low byte]`: two 256-entry tables with the 0.299 / 0.587 / 0.114 weights split by
  byte; white gives 63, and no clamp is needed. As implemented, this replaces the planned three per-channel tables.
- `applySwapped()` writes byte-swapped pixels for the panel in the same pass.
- The result lies on the **black → #00FF66 ramp** (FR-013). #00FF66 in RGB565 is G6 = 63, B5 = 12 (0x66 = 102 →
  102 × 31 / 255 ≈ 12.4 → 12), giving **0x07EC**.
  - Result = `(lum6 << 5) | BR[lum6]`, where the table `BR[l] = round(l × 12 / 63)` (64 entries).
  - Red is always 0.
  - White → 0x07EC; black → 0x0000.
- Black stays black. Ordering is preserved: a brighter input never gives a darker output.
- Black stays black. Ordering is preserved: a brighter input never gives a darker output.
- `void apply(uint16_t* px, size_t n)`: an in-place loop (board flush).

## 6. `core::NvgStyle`

```text
struct AlertLook { bool inverse; bool outline; bool visible; }
AlertLook forLevel(AlertLevel lvl, bool nvg, uint32_t nowMs)
```

| Mode | Warning | Caution | Advisory |
| :-- | :-- | :-- | :-- |
| Day | normal (red bg) | normal (amber bg) | normal (cyan bg) |
| NVG | inverse, visible = (nowMs / 250) % 2 == 0 | inverse, steady | outline, no fill |

The existing 1 Hz flash for unacknowledged alerts still applies on top of this (S4).

## 7. Wake cause (`IBoard::wakeCause()`)

`enum class WakeCause : uint8_t { PowerOn, Button, Imu, Timer, Other }`

- Mapped from `esp_sleep_get_wakeup_cause()`:
  - EXT1 with the `PMU_INT` bit → Button;
  - EXT1 with GPIO8 → Imu;
  - TIMER → Timer;
  - UNDEFINED → PowerOn (also for resets).
- Logged at boot; `ad wake`.

## 8. Pre-sleep hook (`PowerService`)

```text
using PreSleepHook = uint32_t (*)();   // returns the wanted wake-up in seconds (0 = none)
void addPreSleepHook(PreSleepHook);    // ≤4 hooks; S8's chrono registers one
```

`eject()` calls each hook and takes the smallest non-zero value → `deepSleep(mask, timerSec)`.

## 9. Power profile sample (log only, not stored)

`t, pct, mv, chg, vbus, flowPctPerH, screenOn, nvg, rails{lora, amp, gnss, nfc, wifi}, ma (n/a on AXP2101)`

## 10. Events (`EventBus`)

| Event | a | b | From → to |
| :-- | :-- | :-- | :-- |
| `NvgChanged` | 0/1 | – | UI (applySetting) → subscribers (future pages; log) |
| `EjectRequested` | source (0 gesture, 1 console) | – | UI / console → svc (PowerService) |
| `ScreenChanged` | 0 off / 1 on | cause | UI → svc (profiling log) |
| `ScreenCommand` | 0 off, 1 on, 2 raise | – | console (svc) → UI (`ad screen …` test hooks) |
| `AlertRaised` (S4, changed) | AlertId | **level if new/re-armed and unacknowledged, 0 otherwise** | AlertManager → UI (FR-004 wake filter) |

## 11. `ScreenSnapshot` (`Snapshot<ScreenSnapshot>`, written by the UI task only)

```text
bool on; uint8_t cause;            // Boot, Button, Raise, Alert, Touch, Cmd
uint32_t idleMs;                   // time since the last input or wake
uint8_t timeoutIdx; bool raiseEnabled; bool raiseAvailable;
bool nvg; uint8_t brightnessPct;   // as applied (10 in NVG)
uint32_t lastWakeLatencyMs;        // wake decision → first flushed frame
uint32_t seq;
```

Read by `ad screen` and the profiling line (svc task).
