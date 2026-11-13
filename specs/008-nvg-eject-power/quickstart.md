# Quickstart / Validation: NVG Mode, Eject & Power UX (Slice S7)

## Prerequisites

- T-Watch Ultra on COM3 (DTR/RTS low). Firmware is built and flashed:
  ```text
  pio run -e twatch_ultra -t upload
  ```
  `uploadfs` is not needed (no new assets).
- Host tests, with WinLibs GCC on PATH:
  ```text
  pio test -e native
  ```
  All suites green, including `test_screen_policy`, `test_raise_detector`, `test_eject_gesture`, `test_nvg` and
  `test_settings` (v8).
- Clock set (Wi-Fi sync or `ad clock set`), the recorder holding some steps for today (`ad rec sim steps 500` if
  needed).
- **Do not run `ad set reset` on the device** without asking the user (it erases the Wi-Fi credentials).

## Q1 — Screen timeout (US1, SC-001)

1. `ad set scrto 1` (15 s). Leave the watch untouched. Expect `[scr] off (idle 15 s)` within 15–16 s, and the
   panel dark. Repeat 10×.
2. While off, touch the screen: nothing happens, and no navigation happens after it wakes. Also keep a finger on a
   Settings button while raising the wrist or pressing the button: the screen comes on and the button is **not**
   activated (touch suppression after wake).
2a. With the screen off, let an existing alert update (e.g. the fuel % changing under `ad fuel sim`): the screen
   stays off. Only a new or re-armed caution or warning wakes it.
3. Press the button: the screen comes on showing the same page; the page did **not** change to the PFD. The log
   shows `[scr] on (button)`.
4. Keep swiping for 30 s: the screen stays on (touches restart the idle time).
5. Try each `scrto` 0..5; for 5 (Never) the screen stays on for more than 5 minutes.
6. While off, `ad alert raise clocknotset C` (S4 hook): the screen turns on and shows the annunciator.
7. While the Wi-Fi setup screen is shown, the screen does not time out.

## Q2 — Raise-to-wake (US1, SC-001)

1. With the screen off, the arm hanging, raise the wrist into the viewing position 10×. Expect `[scr] on (raise)`
   within 1 s in ≥9 of 10.
2. Walk for 5 minutes with arms swinging: count the false wakes (noted in the power report).
3. `ad set raise 0`: raises no longer wake the screen; the button still does.

## Q3 — Eject and wake (US2, SC-002, SC-003, SC-006)

1. Note `ad rec` (today's steps, Max G and time, history).
2. On the PFD:
   - double-tap the striped handle; it shows ARMED;
   - pull it down more than 40 %;
   - expect the log `[pwr] eject: … sleeping at +<ms> ms` with ms < 2000, and the screen dark.
3. Press the button. Expect the PFD within 4 s, `[boot] wake=button`, and `ad wake` showing the eject time.
   `ad rec` equals step 1, and the settings are unchanged.
4. Repeat steps 2–3 5×.
5. Wrong gestures, 20 in total (expect 0 ejects):
   - single tap (×4);
   - swipe down without tapping (×4);
   - a slow double tap with more than 0.5 s gap (×4);
   - double tap, then wait more than 3 s, then pull (×4);
   - double tap, then pull sideways (×2);
   - pull less than 30 % and release (×2).
6. With an alert active (`ad fuel sim 12` → STALL): the annunciator sits below the handle on the PFD, covering the
   heading row (heading tape, MAX G, FLT NM; these come back when the alert clears), and eject still works.
7. Eject while charging (USB connected): it still sleeps.

## Q4 — Wrist-motion wake spike (R7, SRS OI-1)

1. Build with `-DAERODECK_IMU_WAKE=1`.
2. Check that the hub's virtual-sensor list contains 57 or 67.
3. Eject, then tilt the wrist 10×; count the wakes (`[boot] wake=imu`).
4. Measure the sleep current with and without the flag (Q8 method).
5. Record the result in ADR-014. Promote only if ≥8/10 wakes and the extra current is under 1 mA.

## Q5 — NVG on every page (US3, SC-004)

1. `ad nvg on` (or the Settings toggle):
   - every page (PFD, WX, RECORDER, SETTINGS), the annunciator, the overlay and the eject handle are green on black;
   - `ad screen` shows bright=10%;
   - the wearer walks the ring: no non-green element.
2. Live values keep changing in green: horizon, seconds, recorder, chart.
3. Annunciator in NVG, in a dark room:
   - `ad alert raise stall W`: inverse, blinking fast;
   - `ad alert raise clocknotset C`: inverse, steady once acknowledged;
   - `ad alert raise timeunsync A`: outline;
   - the wearer can tell all three apart.
4. Change the brightness in Settings while in NVG: the value is stored, and the screen stays dim.
5. `ad nvg off`: the colours and the previous brightness return.
6. Reboot with NVG on: the first PFD frame is already green and dim. Eject, then wake: still NVG.

## Q6 — Smoothness (SC-007)

With the overlay on, sample the PFD for 60 s in day mode and in NVG. The median must be **≥30 fps** in both (constitution
IV) and no more than 2 fps below the T002 baseline; record the difference. If NVG misses this, apply the research R1
fallbacks in order and re-measure.

## Q7 — Settings page and persistence (FR-001, FR-017, FR-022)

1. The DISPLAY section has: NVG on/off, Screen timeout −/+ (10 s … Never), Raise to wake on/off (shown as
   unavailable without a motion sensor).
2. Change all three, reboot, and check that they are kept.
3. Restore defaults gives NVG off, 15 s, raise on. **On the device only with the user's consent**; otherwise it is
   covered by `test_settings`.

## Q8 — Power profile and report (US4, SC-005, SC-006)

1. `ad pwr log on`: one `[pwr] prof …` line per second; `ad pwr log off` stops it.
2. Off USB, per state (≥2 h each where practical):
   - PFD on, day: `ad set scrto 5`;
   - PFD on, NVG;
   - screen off idle: the default timeout, the watch at rest;
   - ejected: ≥24 h, read the % before and after.
3. Optional: a USB meter with the battery full, per state.
4. Write `docs/power_report.md`: method, state currents, the typical-day profile (research R9), the estimate vs.
   the ≤50 mA / ≥22 h target, the verdict and next steps.
5. Check SC-006: ejected current ≤ 1/10 of screen-off idle.
