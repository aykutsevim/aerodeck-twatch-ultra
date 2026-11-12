# Validation: G-Force & Flight Recorder (S6)

Device: T-Watch Ultra on COM3, branch `007-flight-recorder`, 2026-11-12. Scripts (session scratchpad):
`validate_s6.py`, `flick_test.py`, `midnight_latency.py`, plus the S1/S4/S5 regression scripts.

## T002 · Baseline
`pio test -e native` 184/184 (the 4 new empty test folders errored as expected); `twatch_ultra` ✅, `twatchs3` ✅.

## T003–T009 · Host tests
`pio test -e native`: **205/205** (184 → 205: test_day_log 9, test_step_acc 4, test_flight_math 4, test_rec_blob 3,
test_settings +1). Unity double asserts are disabled in this build → NM compared as float.

## T012 · Motion probe (research R1/R2)
| check | result |
|---|---|
| step counter (STC 52) in the KLIO firmware | ✅ present (`steps=OK`); no fallback needed |
| accelerometer | ✅ `accel=OK`, `configureRange(16)` accepted (range 16) |
| scale | ❌→✅ `getScaling` stayed 1/4096 after the range change: at rest `g_now=0.50`. Fixed: scale = range/32768 → **1.00 g** at rest |
| start-up transient | a ≈2.0 g peak logged ~1 s after configure on every boot (watch at rest) → peaks in the first 2 s discarded → boot Max G 1.0 |
| walking | wearer walked ~40 steps → **34** counted (the hub holds back the first steps until it detects walking) |
| boot order | motion configured from `onImuReady` after the deferred probe (≈3.6 s after boot), per analysis U1 |

## T015 · US1 Flight Miles (Q2)
- `ad rec sim steps 2470` at 0.75 m → NM +1.00 (1.02 → 2.02) ✅; PFD FLT NM follows within 1 s (snapshot each
  second).
- `ad set stride 80` → NM recomputed from steps (2.16, expected 2.16), steps unchanged ✅; `stride 151` rejected ✅.
- Settings → RECORDER → Stride −/+ (5 cm) and the PFD following: **wearer "Works"** ✅.
- **SC-001 (3 walks ≥500 m): pending** — the wearer chose to do it later (listed in the roadmap).

## T017 · US2 Max G (Q3, SC-002)
- At rest `g_now` 1.00 ✅; `sim g 5.5` → `maxg=5.5@13:36` ✅; `sim g 1.2` → `OK lower` ✅.
- **10 wrist flicks** (`flick_test.py`, buzz prompt, clear between rounds): **10/10 above 2.0 g** (4.2 … 16.0 g;
  16.0 = range limit, shown "16.0+"), minute recorded = PC minute in every round ✅. Seen 1.3–2.5 s after the buzz
  (includes the wearer's reaction; the recorder polls at 1 Hz).
- A first run reported 5/10: the script only printed its last read and could not tell missed flicks from failed
  reads; the diagnostic re-run (best value, read count, the watch's own `max g` lines) gave 10/10.

## T018/T019 · US3 persistence (Q4, SC-003)
- `ad rec save` → `saved (cmd) 6 ms` (72 ms once, with NVS page maintenance).
- **10/10 planned reboots** (`ad reboot`, shutdown handler): steps, Max G with its minute and the history unchanged
  ✅. (Two rounds first flagged FAIL only because the `start today=` boot line was dropped by USB CDC; the values
  were identical. The criterion now uses the values only.)
- Settings restore: `ad set reset` → steps/Max G/history unchanged, stride back to 75 ✅. **Side effect:** restore
  defaults also erased the saved Wi-Fi network (S2 behaviour) — this should not have been run on the wearer's watch
  without asking; the wearer has to re-provision Wi-Fi. Clock set from the PC (`ad time set`) meanwhile.
- **Power cut** (wearer: USB unplugged, power button held until off, powered on): `reset_reason=poweron`; 3 500 saved
  steps kept, the 111 unsaved lost, history intact ✅.
- A USB flash is a hardware reset (no shutdown handler): observed once — the 34 walk steps after the last periodic
  save were lost, as designed (≤10 min).

## T020/T021 · US4 Recorder page (Q6, SC-005)
- Page ring (`validate_s1.py regress`): PFD → WX → **RECORDER** → SETTINGS → PFD ✅.
- Demo week (simulated dates Thu 12 → Wed 18 Nov, one day without steps): **wearer "Correct & readable"** — PFD
  MAX G 2.7 / FLT NM 1.42 = page values = `ad rec`; 7 bars, Mon tallest, Sat outline, today green; tapping a bar shows
  that day ✅.
- Sensor failure (`ad simfail imu` + reboot): `steps_src=absent accel=absent`, IMU FAIL caution, ATT FAIL, history
  kept; **wearer**: PFD "MAX G --" / "FLT NM --", page shows "Motion sensor unavailable" and the chart ✅. Fixed during
  the check: page and console showed the stored Max G (1.0) instead of "--" → now "--" whenever the accelerometer is
  unavailable (FR-005). Restored (`simfail none`).

## T022/T023 · US5 rollover (Q5, SC-004)
| case | result |
|---|---|
| `ad rec rollover` ×8 | d-1 = 800 steps, d-7 = 200; the oldest dropped; today 0 ✅ |
| live midnight ×3 (23:59:50 local, Nov 20/21/22) | `[rec] rollover` each time, today 0, the finished day = d-1 ✅ |
| latency | rollover **151 ms after midnight** (log timestamps; FR-006 ≤2 s) ✅ |
| clock set back across midnight | no rollover, today unchanged ✅ |
| time zone Istanbul → Tokyo (date moves forward) | exactly 1 rollover; back to Istanbul: none ✅ |
| clock +3 days | `rollover … (empty=2)` ✅ |

Test data cleared afterwards (`ad rec clear`); the recorder starts clean on the real date.

## T026 · Regression and performance
- `pio test -e native` 205/205; `twatch_ultra` ✅ (RAM 19.8 %, flash 48.4 %); `twatchs3` ✅.
- Layering: no `instance.` / SensorLib / `bhy2_` / LilyGoLib outside `src/board/`; no `lv_` in services/core;
  `Preferences` only in services ✅.
- S0/S1 regression ✅; S4 ordering 10/10, ack stops the shaker, a new caution after ack plays ✅ (one log line
  dropped per run, alternating); S5 callouts 30/30 (134–135 ms) and amplifier 20/20 ≤406 ms ✅.
- **SC-006:** PFD + `att sweep on` with the recorder running: median **29 fps** (min 28), with audio playing 30 fps;
  heap min 102 KB (Wi-Fi not configured at the time) ✅.
