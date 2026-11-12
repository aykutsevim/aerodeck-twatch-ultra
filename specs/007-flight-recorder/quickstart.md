# Quickstart & Validation: G-Force & Flight Recorder (S6)

**Prerequisites:** as in S5. The walking and flick checks need the wearer.

## Q1. Build and tests
```powershell
pio test -e native                    # + test_day_log, test_step_acc, test_flight_math, test_rec_blob, settings v7
pio run -e twatch_ultra -t upload --upload-port COM3
pio run -e twatchs3
```
`ad rec` → `steps_src=OK accel=OK`, `g_now` 1.00 ± 0.05 at rest (R2 scale check).

## Q2. Flight Miles (US1, SC-001)
- `ad rec sim steps 2470` at stride 75 → `nm` +1.00; PFD FLT NM shows it within 1 s.
- `ad set stride 80` → today's NM recomputed (steps unchanged); restore 75.
- **Wearer:** 3 walks of ≥500 m on a measured route with the stride set to their real stride → NM within ±15 %.
- Step counter after `ad reboot` restarts at 0 and today's steps continue (no drop, no double count).

## Q3. Max G (US2, SC-002)
- At rest: `ad rec` `g_now` ≈ 1.0; MAX G ≈ 1.0–1.5 after quiet use.
- **Wearer:** 10 sharp flicks (check MAX G after each; reset between with `ad rec clear` or compare increases) →
  above 2.0 within 1 s; the recorded minute matches.
- `ad rec sim g 1.2` after a higher peak → `OK lower`.

## Q4. Persistence (US3, SC-003)
- Script: 10 × (`ad rec sim steps 100`, `ad rec sim g 2.0+i/10`, `ad reboot`, `ad rec`) → steps, Max G, time and
  history unchanged after each reboot (`saved (shutdown)` logged).
- Power loss: add steps, wait for a `saved (periodic)` line, add more, hold the power button to cut power (or unplug
  battery+USB) → after restart only the steps after the last save are missing (≤10 min of data).
- Settings → Restore defaults → `ad rec` unchanged; stride back to 75.

## Q5. Rollover (US5, SC-004)
- `ad time set` to 23:59:50 local (UTC = local − tz) → within 2 s after midnight: `[rec] rollover`, today = 0, the
  finished day is `d-1`. ×3.
- Clock moved back across midnight → no rollover. Time zone change (`ad tz set …`) that keeps the date → none.
- `ad rec rollover` ×8 → history keeps 7 days, the oldest dropped.
- Clock jump forward by 3 days → 2 empty days inserted.
- Restore the clock with `ad sync`.

## Q6. Recorder page (US4, SC-005)
- Swipe PFD → WX → RECORDER → SETTINGS. Values match the PFD and `ad rec` (×5).
- Chart: 7 bars, today highlighted, weekday letters, empty days visible; tap a bar → its values. The wearer judges
  readability.

## Q7. Performance (SC-006)
- PFD + `ad att sweep on`, recorder running, plus a burst of `ad rec sim steps 1` → `ad stat` FPS median ≥28, heap
  ≥40 KB.
