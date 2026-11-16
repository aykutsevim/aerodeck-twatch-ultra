# Research: Drop-Tank Chronograph (Slice S8)

**Date**: 2026-11-13 · **Spec**: [spec.md](./spec.md) · **Plan**: [plan.md](./plan.md)

Sources:
- AeroDeck `TimeService` (system clock set from the RTC at boot), `AlertManager`/`core::Alerts`, `PowerService`
  pre-sleep hooks and `BoardUltra::deepSleep` (S7, ADR-014);
- SensorLib `SensorPCF85063.hpp` (alarm API);
- the T-Watch Ultra variant pins (`RTC_INT` = GPIO1, `PMU_INT` = GPIO7).

---

## R1 — Time base: absolute UTC end time, not tick counting

**Decision:** a running timer stores its **target end time in UTC milliseconds** (from `gettimeofday`). A paused
timer stores its **remaining milliseconds**. Remaining = target − now, clamped to [0, interval length].

**Rationale:**
- System time survives page changes, the screen being off and restarts.
- After a deep sleep the boot re-sets the system clock from the RTC chip (S2), so the target time is still
  meaningful after eject.
- Time zone changes do not matter (UTC).
- An NTP correction of Δ shifts the remaining time by Δ (FR-009 allows that).
- Clamping protects against a clock reset after a power loss: with the RTC back at 2000-01-01, the remaining time
  is capped at the interval length, never years.
- **Resolution (U1):**
  - At boot the system clock is set from the RTC's whole seconds, so it can be up to 1 s behind.
  - **Decision:** `TimeService` sets the system clock to the RTC seconds **+ 500 ms**, which centres the error at
    ±0.5 s.
  - Waiting for the RTC's second rollover was rejected: it costs up to 1 s of boot time.
  - With the 50 ms svc tick, an alert after a wake lands within ±0.55 s of the target. Running awake, it is exact
    to 50 ms.

**Alternatives:**
- *`millis()` counting:* lost on every restart and deep sleep.
- *An RTC_DATA_ATTR mirror:* lost on power loss, and redundant with NVS (ADR-014).

## R2 — Waking from deep sleep at the right moment

**Finding:**
- The ESP32-S3 deep-sleep timer runs on the internal RC slow clock. After calibration it is still typically 1–5 %
  off and temperature-dependent, so a 120-minute sleep could wake minutes late.
- The **PCF85063 RTC** has its own crystal and an alarm (second, minute, hour and day match;
  `SensorPCF85063::setAlarm`). Its INT output goes to `RTC_INT` = **GPIO1**, an RTC-capable GPIO, active-low open
  drain. That matches the ext1 `ANY_LOW` mode already used for `PMU_INT`.

**Decision:**
- `BoardUltra::deepSleep(mask, timerSec)` with `kWakeTimer` sets the **RTC alarm** at RTC-now + `timerSec`
  (day/hour/minute/second match; at most 31 days ahead), clears the alarm flag, enables the alarm interrupt, and adds
  GPIO1 to the ext1 mask. GPIO1 is then left out of the "open-drain unused pins" list.
- **Pull-up (U2):** the PCF85063 INT output is open-drain. In deep sleep the board enables GPIO1's RTC pull-up
  (`rtc_gpio_pullup_en`, `rtc_gpio_pulldown_dis`) unless an external pull-up is confirmed. Q0 checks for no false
  instant wake.
- **RTC integrity (U3):** the alarm is **relative** to the RTC's current reading (RTC-now + `timerSec`), so it works
  even when the clock integrity flag is false (time never set). The ESP timer fallback is only for a **missing or
  failed RTC chip** (`periph(Rtc) != Ok`).
- If the RTC is unavailable, it falls back to `esp_sleep_enable_timer_wakeup` (and logs the inaccuracy).
- `wakeCause()` maps ext1 with GPIO1 to `WakeCause::Timer`. At boot the board clears the alarm flag and disables
  the alarm.
- No `IBoard` signature change: the board chooses the wake mechanism.

**Rationale:** the crystal keeps the ±1 s target over 2 hours. Only the IBoard implementation changes.

**Alternatives:**
- *ESP timer with a proportional safety margin, re-sleeping after early wakes:* each wake is a 3.5 s boot that
  lights the screen. Rejected.
- *Staying awake (light sleep) during short timers:* no light-sleep path exists (S7 decision). Rejected.

## R3 — Boot lead time and very short remaining times

**Finding:** a wake is a full boot that takes about 3.4 s to the watch face (S7 `ad boot`).

**Decision:**
- The Chrono pre-sleep hook asks to wake at **target − 5 s**. After the boot, `ChronoService` fires at the target
  on its 50 ms tick, so there is no late alert caused by the boot.
- If the remaining time at eject is **≤ 6 s**, the hook returns `PowerService::kPostponeSleep`, a new sentinel.
  `eject()` then aborts: it logs `[pwr] eject postponed: timer expiring`, and the UI resumes (the S7 "unsupported"
  path). The alert then fires awake, on time. This is the spec edge case "alerts before sleeping".
- **Pomodoro:** the hook uses the next **transition** time.

**Rationale:** it is simple and keeps the ±1 s promise. The eject gesture is rarely used in the last 6 s of a timer.

## R4 — Alerts

**Decision:** two new alert ids in `core::Alerts`.
- `BingoFuel` = 42: Caution, `Pattern::Caution`, clip `Clip::BingoFuel` (already present from S5), text
  "BINGO FUEL".
- `BreakOver` = 43: Caution, `Pattern::Caution`, clip `Clip::Chime`, text "BREAK OVER".
- Raised by `ChronoService` with `AlertManager::raise()`, so quiet mode, quiet hours, acknowledge, the annunciator,
  NVG looks and the S7 screen wake all apply unchanged.
- Cleared on reset or start (FR-006). In Pomodoro, a transition clears the previous phase's alert first.
- A **late** expiry (found at boot) is raised with the text "BINGO FUEL (LATE)".

## R5 — Persistence

**Decision:**
- **Timer state:** its own NVS namespace `chrono`, blob `st` (versioned, CRC-16 like the S6 `RecBlob`). It is
  written only on state changes (start, pause, resume, reset, phase change, expiry): a few writes per session.
  Restore defaults keeps it.
- **Settings v9:**
  - `chrlen`: countdown minutes 1..120, default 25;
  - `chrmode`: 0 countdown, 1 Pomodoro;
  - `chrwork`: Pomodoro work minutes 1..120, default 25;
  - `chrbrk`: break minutes 5..15 step 5, default 5.
- **At boot:** `ChronoService::begin` loads the blob.
  - Running and the target has passed: Expired, with the alert raised late.
  - Pomodoro: the missed transitions are applied, at most one alert, which names the current phase.

## R6 — The page and the tank widget

**Decision:**
- `PageId::Chrono` sits between Recorder and Settings.
- The page has:
  - a **tank** widget: a custom-drawn rounded drop-tank silhouette (body, nose cone, fins) with a fill clipped to
    the remaining fraction, redrawn once per second (≤ 1 Hz, no animation needed);
  - the MM:SS readout in the DSEG7 40 px font (as S6);
  - a phase label (COUNTDOWN / WORK / BREAK / BINGO) and the cycle count;
  - a row of preset buttons (5 / 15 / 25 / 50) with custom −/+;
  - a Countdown | Pomodoro mode switch;
  - Start/Pause and Reset buttons, ≥ 48 px.
- Colours: theme tokens (fuel = HUD green, low < 10 % = caution, empty = warning). The NVG flush filter handles
  night mode (S7).
- **Rationale:** a static redraw at 1 Hz costs nothing next to the PFD. It is not rendered while the page is
  hidden.

## R7 — Where the logic lives

**Decision:**
- Pure `core::ChronoModel` holds the state machine on absolute times, Pomodoro phases, `remaining`, `fraction`,
  `nextEventUtcMs`, and `restore(now)`. It is host-tested.
- `ChronoService` (svc tick at 50 ms) adds the NVS, the alerts, the snapshot, the pre-sleep hook and the console.
- **Idle cost (C1):** when Idle, Paused or Expired, `tick()` only compares the state and returns. It does no NVS
  write and no snapshot republish unless something changed. NVS is written only on state changes (C2).
- `ChronoPage` (UI) reads `ChronoSnapshot` and calls the thread-safe `ChronoService` commands (start, pause, resume,
  reset, set length or mode), queued to the svc task.
