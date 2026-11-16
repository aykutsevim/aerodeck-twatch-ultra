# Quickstart / Validation: Drop-Tank Chronograph (Slice S8)

## Prerequisites

- T-Watch Ultra on COM3 (DTR/RTS low), firmware flashed.
- `pio test -e native` all green: `test_chrono_model`, `test_chrono_blob`, `test_settings` v9, alert ids.
- The watch clock set (it is; the RTC keeps UTC).
- **Never run `ad set reset` on the device without asking the user.**

## Q0 — RTC-alarm wake probe (R2)

`ad probe s8 alarm 60` → the watch sleeps. It must wake by itself 60 ± 1 s later (PC clock), with `ad wake` showing
`wake=timer`. Repeat with 300 s. The watch must not wake early (no false wake through a floating INT line; the
GPIO1 pull-up is on).

## Q1 — Countdown basics (US1)

1. On the CHRONO page, pick 25: shows 25:00, tank full. Start: counts down; at 12:30 the tank is half full
   (`ad chrono` frac ≈ 0.50).
2. Pause → wait 30 s → resume: the remaining time is unchanged (±1 s). 5 trials, pauses of 5–60 s.
3. Reset: back to the set time, full tank. Custom −/+ reaches 1 … 120 min. The presets are locked while running.

## Q2 — Expiry accuracy (US2, US3, SC-001)

A script runs `ad chrono start 60` and compares the `[chrono] expired` log and the alert against the PC clock.
3 runs each case:
1. CHRONO page shown;
2. another page;
3. screen off (timeout 10 s; the alert turns the screen on);
4. **ejected** right after starting: the watch wakes by itself, boots, and alerts at 60 ± 1 s.

Each run: BINGO FUEL caution, vibration and "Bingo Fuel" voice. Acknowledge stops them; reset clears the caution.

## Q3 — Eject edge cases (US3)

- Eject with ≤ 6 s left: `[pwr] eject postponed: timer expiring`; the watch stays awake and alerts on time.
- Eject, then button-wake before the end: still running, correct remaining time.
- Pause, eject, wake: still paused, same remaining time.

## Q4 — Restart (US3, SC-004)

- `ad reboot` mid-timer: the remaining time is still correct (±1 s).
- Start 60 s, eject, wait until well past the target, then button-wake:
  - if the RTC alarm woke it first, it alerted on time;
  - otherwise use `ad probe s7 sleep` (button-only wake) to force the "late" case: `BINGO FUEL (LATE)` at boot.

## Q5 — Pomodoro (US4, SC-003)

`ad chrono pomo 60 60`:
- WORK 1:00 → BINGO FUEL (voice) → BREAK 1:00 → BREAK OVER (chime) → WORK …;
- the cycle count increments;
- one transition happens while ejected (the watch wakes for it);
- reset ends the session.

## Q6 — Wearer (SC-005)

Set, start, pause and reset in < 10 s. The tank and MM:SS read at a glance, in day mode and in NVG.

## Q7 — Smoothness (SC-006)

PFD median ≥ 30 fps over 60 s with a timer running (`fps_s7.py`).
