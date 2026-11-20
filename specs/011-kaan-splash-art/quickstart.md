# Quickstart / Validation: KAAN Splash Screen and Watch-Face Emblem

## Prerequisites

- T-Watch Ultra on COM3; the PlatformIO Python (`%USERPROFILE%\.platformio\penv\Scripts\python.exe -m platformio`).
- Asset regeneration only (not needed to build): Windows PowerShell 5.1 (.NET System.Drawing).

## Q0 — Assets and off-device tests

```text
python tools/gen_kaan_art.py           # regenerates src/assets/KaanArt.{h,cpp}; a second run gives no diff
pio test -e native                     # SplashCompose, PfdLayout.emblem, asset sanity tests
pio run -e twatch_ultra ; pio run -e twatchs3
```

Expected:
- all tests pass;
- both builds succeed;
- flash grows by ≈ 150 KB on the Ultra (the A8 masks); RAM is unchanged apart from a few bytes.

## Q1 — Splash at full start (US1, SC-001, SC-003)

- `ad reboot` 5 times (each a full start; a cold power-on via the PMU power key is optional; eject is deep sleep, see
  Q2): the KAAN art appears full-screen, centred, before the PFD.
- After each start, `ad boot` shows `splash=1`, `splash_at_ms`, and **`boot_to_home_ms` ≤ 4000**. Record the visible
  window (`boot_to_home_ms − splash_at_ms`) and `splash_cost_ms`.
- Wearer: the whole aircraft and "KAAN" are visible, undistorted, not cut by the corners.
- NVG on (`ad nvg on`), `ad reboot`: the splash is green and dim (10 %), with no bright white flash. Then `ad nvg off`.

## Q2 — No splash on wake from eject (FR-004, SC-002)

- Eject (`ad eject` or the gesture), wake with the button, 3 times: no splash; `ad boot` → `splash=0 skip=wake`;
  the mean `boot_to_home_ms` is within 50 ms of the wake value
  measured before the change (T001).
- With a S8 timer wake (`ad probe s8 alarm 20`): `skip=wake`.

## Q3 — Robustness (FR-005)

- Safe mode is not provoked on purpose. Code review plus the unit test of the show rule cover it
  (`skip=safe`).
- `ad simfail rtc` (then `ad simfail mic`) + `ad reboot`: the splash still shows, then the PFD with the usual CAUTION, and the
  skipped peripheral is reported as before. Then `ad simfail none` + `ad reboot`.

## Q4 — PFD emblem (US2, SC-004..006)

- The PFD shows the emblem at the bottom centre between ZULU and LOCAL, with no overlap (wearer + photo/description).
- `ad att sweep on` + `ad stat` ×3: median fps ≥ 30 (SC-005). Heap ≥ 40 KB.
- `ad nvg on`: the emblem is green, not white (SC-006). `ad nvg off`.
- Wearer: is the emblem recognisable? Is the "KAAN" lettering acceptable at that size? If not → regenerate with
  `--emblem-aircraft-only` (contracts/art_assets.md).

## Q5 — Regression

- Swipe through all pages; the eject handle on the PFD still works (top band); `ad boot` wake timings as in S7/S8.
- Init-split regression (touch, RTC, mic and amp now start after the splash):
  - `ad audio play traffic` plays;
  - a tap works right after a button wake;
  - the boot log shows `[time] RTC … valid` and `ad time` is correct;
  - `ad periph` is unchanged from before the change.
