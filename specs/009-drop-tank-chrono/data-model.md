# Data Model: Drop-Tank Chronograph (Slice S8)

All `core::` types are pure and host-tested. Times are UTC milliseconds (`int64_t`).

## 1. Settings v9 (`core::SettingDefs`, NVS `aerodeck`)

| Key | Type | Default | Range | Meaning |
| :-- | :-- | :-- | :-- | :-- |
| `chrlen` | U8 | 25 | 1..120 | Countdown length (minutes; presets 5/15/25/50 or custom) |
| `chrmode` | Bool | 0 | 0..1 | 0 countdown, 1 Pomodoro |
| `chrwork` | U8 | 25 | 1..120 | Pomodoro work length (minutes) |
| `chrbrk` | U8 | 5 | 5..15 step 5 | Pomodoro break length (minutes) |

`kSchemaVersion` = 9; migration v8 → v9 adds the defaults.

## 2. `core::ChronoModel`

```text
enum Mode  { Countdown, Pomodoro }
enum State { Idle, Running, Paused, Expired }
enum Phase { Countdown, Work, Break }
enum Event { None, Expired, WorkDone, BreakDone }   // returned by tick()

Config  { Mode mode; int64 countdownMs, workMs, breakMs }
Fields  state, phase, intervalMs (current phase length), endUtcMs (Running), remainingMs (Paused),
        cycles (completed work intervals), late (bool)

start(now)       Paused → same as resume(now) (I2); Idle/Expired → Running; phase = Countdown or Work; endUtcMs = now + intervalMs; cycles = 0 on a new
                 session
pause(now)       Running → Paused; remainingMs = clamp(endUtcMs − now)
resume(now)      Paused → Running; endUtcMs = now + remainingMs
reset()          any → Idle; phase = first phase; remaining = interval; late = false
tick(now)        Running and now ≥ endUtcMs:
                   Countdown → Expired, Event::Expired
                   Work      → Break: cycles++, endUtcMs += breakMs, Event::WorkDone
                   Break     → Work:  endUtcMs += workMs, Event::BreakDone
remainingMs(now) clamp(endUtcMs − now, 0, intervalMs) when Running; remainingMs when Paused; intervalMs when Idle;
                 0 when Expired
fraction(now)    remainingMs / intervalMs
nextEventUtcMs() endUtcMs when Running, else none (−1)
restore(now)     after a load: Running with endUtcMs ≤ now:
                   Countdown → Expired, late = true (Event::Expired once)
                   Pomodoro  → advance whole phases until endUtcMs > now (cycles counted); late = true; one event for
                               the current phase
                 Running with endUtcMs − now > intervalMs (clock reset backwards) → clamp: endUtcMs = now + intervalMs
```

**Terms (I3):**
- **interval** = the length of the current phase (`intervalMs`).
- **countdown length** = the setting `chrlen` (`countdownMs`).
- **work/break length** = `chrwork` / `chrbrk`.
- The spec's "set time" is the countdown length.

**Rules:**
- Pomodoro transitions keep the schedule: `endUtcMs += next length`, measured from the previous end, so there is no
  drift from tick latency.
- `start` while Expired clears `late`.

## 3. `core::ChronoBlob` (NVS `chrono`, key `st`)

`version(1) mode(1) state(1) phase(1) cycles(2) late(1) pad(1) intervalMs(4) endUtcMs(8) remainingMs(4)
countdownMs(4) workMs(4) breakMs(4) crc16(2)`: 38 bytes, CRC-16/CCITT-FALSE (as `RecBlob`).
- An unknown version, a wrong length or a CRC mismatch is rejected, and the timer stays Idle.

## 4. `ChronoSnapshot` (svc task → page, console)

`state, mode, phase, remainingMs, intervalMs, fraction, cycles, late, endUtcMs, seq`

## 5. Alerts (`core::Alerts`)

| Id | Name | Level | Pattern | Clip | Text |
| :-: | :-- | :-- | :-- | :-- | :-- |
| 42 | `bingofuel` | Caution | Caution | `BingoFuel` | "BINGO FUEL" (late: "BINGO FUEL (LATE)") |
| 43 | `breakover` | Caution | Caution | `Chime` | "BREAK OVER" |

## 6. Pre-sleep hook (S7 `PowerService::PreSleepHook`, extended)

- Returns the seconds to wake in, or 0 for no wake.
- **New:** `kPostponeSleep` (UINT32_MAX) means "do not sleep now". `eject()` aborts and the UI resumes.
- Chrono's hook:
  - not running: 0;
  - running with r = (nextEvent − now) ≤ 6 s: `kPostponeSleep`;
  - otherwise: `r − 5` s, rounded down, minimum 1.

## 7. Board timer wake (internal, `BoardUltra::deepSleep`)

- `kWakeTimer` with `timerSec`: PCF85063 alarm at RTC-now + `timerSec` (match day/hour/minute/second); clear the AF
  flag; enable the alarm interrupt; ext1 mask |= GPIO1 (`RTC_INT`); GPIO1 is not open-drained.
- Fallback without the RTC: `esp_sleep_enable_timer_wakeup`.
- At boot: ext1 status bit GPIO1 → `WakeCause::Timer`; reset the alarm flag and disable the alarm.
