# Validation: Drop-Tank Chronograph (Slice S8)

Device: T-Watch Ultra on COM3. Branch `009-drop-tank-chrono`, started 2026-11-13.

## T001 — Baseline

Taken from the S7 final checks: the same code, merged as `main` e6f960d.
- `pio test -e native`: **251/251**.
- `twatch_ultra`: RAM 66 900 B, Flash 2 053 069 B. `twatchs3`: SUCCESS.
- PFD fps: median **35** (day and NVG).

## T002/T003 — Q0: RTC-alarm wake probe

- `ad screen off` + `ad probe s8 alarm 60`: the watch woke **by itself**. `wake=timer`. The USB port came back
  **59.67 s** after the command (the alarm matches the RTC's whole second, so the wake falls 59–60 s later).
- `ad probe s8 alarm 300`: woke by itself, `wake=timer`, port back after **299.84 s**.
- No early wake in either run: the GPIO1 RTC pull-up holds the open-drain INT line.
- **Conclusion:** the crystal alarm is accurate within the RTC's 1 s resolution over 5 minutes; R2 is confirmed.
- Found while editing: mojibake (`Â±`, `Ã—`, `Âµ`) in comments of `src/board/BoardUltra.cpp`, from S7 PowerShell
  `Get-Content/Set-Content` edits. Fixed. The S7 I2C scan's 100 kHz clock change had not actually been applied
  (CRLF mismatch); added now.

## Core (T004–T009)

- `test_chrono_model` 14, `test_chrono_blob` 2, settings v9 and chrono alerts added.
- `pio test -e native`: **269/269**.

## T014 — Q1 Countdown basics (US1)

- **Wearer:** "Looks good" (page layout, after the first 20 s test timer); "Easy and readable" (set / start /
  pause / reset in < 10 s, −/+ and Pomodoro + BRK).
- **Midpoint** (`ad chrono start 60`, read at 30 s): rem 30.00 s, frac **0.50**.
- **Pause/resume** (`chrono_q1b.py`, 5 trials, pauses of 5/10/20/40/60 s):
  - remaining unchanged while paused;
  - the time lost while running between resume and pause matched the PC clock to within **0.012–0.039 s**.

## T016 / T019 — Q2 Expiry accuracy (US2, US3, SC-001)

Script `chrono_expiry.py`, 60 s timers, 3 runs per case. "Device err" is the time between the target and the alert
on the watch's own clock. "PC" is when the poll first saw `state=expired`; lost USB replies inflate it.

| Case | Device err | PC-observed | Notes |
| :-- | :-- | :-- | :-- |
| CHRONO page | 52 / 1 / 49 ms | +0.92 / +0.31 / +0.14 s | |
| Other page (PFD) | 5 / 1 / 51 ms | +0.10 / **+1.34** / +0.14 s | 1.34 s = lost poll replies (device err 1 ms) |
| Screen off (10 s timeout) | 2 / 1 / 2 ms | +0.62 / **+1.68** / +0.08 s | the alert turned the screen on (`cause=alert`) |
| **Ejected** after 2 s | 46 / 47 / 12 ms | **−0.13 / −0.53 / +0.29 s** | woke itself (`wake=timer`, `wake_in=52`) |

- One more eject run (120 s, wearer slow to press) also alerted on time via the RTC wake: device err 342 ms.
- **Wearer:** vibration and "Bingo Fuel" at expiry ("Looks good"); BINGO FUEL caution id 42 raised and acknowledged.

## T020 — Q3/Q4 Continuity (US3, SC-004)

- **Eject with 4.1 s left:** `sleep hook: postpone (4098 ms left)`, eject postponed; the watch stayed awake and
  expired on time.
- **Button wake mid-timer** (300 s timer, ejected, the wearer pressed after about 3 min): still running, rem 108.4 s
  vs 107 s from the PC clock (start-time uncertainty ~1 s).
- **Paused + eject + wake:** still paused at **01:37.720** (unchanged); no timer wake armed (`wake_in=0`).
- **`ad reboot` mid-timer:** remaining before 114.80 s, after 105.40 s, PC elapsed 9.61 s → **−0.21 s**.
- **Late:** a 30 s timer, then a raw button-only deep sleep (no hook), woken > 45 s later. Wearer: "BINGO FUEL
  (LATE)" at boot.
- **NVS writes (C2):** 0 saves during 60 s of running; pause +1, resume +1.

## T022 — Q5 Pomodoro (US4, SC-003)

- `ad chrono pomo 15 15` trace:
  - WORK → at 15 s BINGO FUEL, cycle 1, BREAK;
  - at 30 s BREAK OVER, WORK;
  - at 45 s BINGO FUEL again;
  - alerts are swapped at each transition.
- `ad chrono pomo 60 60` with an **eject at 70 s** (during the break): the watch slept with `wake_in=44`, woke itself
  (`wake=timer`) and raised **BREAK OVER** on time (not late). The next WORK ran on schedule (56.8 s left, cycle 1).
- Reset ends the session (idle, cycle 0, alerts cleared).

## T026 — Q6/Q7 Wearer, NVG, smoothness, idle drain

- **Wearer, day:** "Easy and readable". **NVG** (timer running): "Readable in NVG".
- **PFD with a timer running:** median **35 fps** (min 33), the same as the baseline. Free heap 84 KB.
- **Idle drain (FR-015, USB meter, screen off):** timer running **0.034 A**, timer idle **0.034 A**, so no
  difference.

## T027 — Final checks

- `pio test -e native` **269/269**.
- `twatch_ultra` RAM 67 596 B (+696), Flash 2 064 701 B (+11 801).
- `twatchs3` SUCCESS.

## Success criteria

| SC | Result | Evidence |
| :-- | :-- | :-- |
| SC-001 | ✅ | Device err ≤ 52 ms in all awake cases; ejected −0.53…+0.29 s vs the PC clock |
| SC-002 | ✅ | Pause/resume diff ≤ 0.04 s in 5/5 |
| SC-003 | ✅ | Pomodoro W→B→W→B with alerts and cycles, incl. a transition while ejected |
| SC-004 | ✅ | Reboot −0.21 s; late expiry raised at boot |
| SC-005 | ✅ | Wearer: "Easy and readable", "Readable in NVG" |
| SC-006 | ✅ | PFD 35 fps with a timer running |

**Pending manual checks:** none.
