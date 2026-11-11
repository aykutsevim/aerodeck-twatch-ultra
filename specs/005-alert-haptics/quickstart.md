# Quickstart & Validation: Alert Manager & Haptics (S4)

**Prerequisites:** as in S3 (PlatformIO via `python -m platformio`, WinLibs GCC on PATH, watch on COM3, serial opened
with DTR/RTS low). Haptic checks need the wearer (feel).

## Q1. Build & tests
```powershell
pio test -e native     # + test_alerts (AlertSet, table), test_battery_alarm, test_haptic_policy, test_settings v5
pio run -e twatch_ultra -t upload --upload-port COM3
pio run -e twatchs3
```

## Q2. Migration and retrofit (FR-004)
- `ad set list` → `hapticlvl=1 quiet=0`; the S0–S3 settings are unchanged.
- Boot with `ad simfail imu` → band "IMU FAIL" amber; `ad alert` lists it as CAUTION.
- `ad time set 2020-01-01T00:00:00Z` → "CLOCK NOT SET" CAUTION. `ad sync` → cleared.
- `ad wifi age 90000` → "TIME UNSYNC" ADVISORY (cyan).

## Q3. Ordering and band (US1, SC-002)
- Raise `test1 A`, `test2 C`, `clocknotset`, `stall`, and `test3 C` in random orders, 10 times (script). The master is
  always STALL (WARNING); `ad alert` shows level-then-time order; the band shows `STALL  +4`.
- `ad alert ack` → steady; cycles through the 5 every 3 s (watch the band).
- Clear them one by one → the band follows within 1 s and hides at zero.

## Q4. Battery sweep: STALL / PULL UP (US2, SC-001)
`ad fuel sim <p> nochg` stepping 20 → 3 → 20 (1 step/2 s), then `chg`. Expected:
- STALL raised once at 14;
- PULL UP once at 5;
- de-escalation to STALL (acknowledged) at 7;
- clear at 17;
- `chg` clears everything immediately;
- no flicker at 15/16.

Run 3 times (SC-001). With intensity off (`ad set hapticlvl 0`), STALL still vibrates.

## Q5. Acknowledge (US3, SC-003)
- `stall` raised → the shaker repeats every ~2 s.
- **Tap the band** → vibration stops within 1 s, the band turns steady, and the page underneath does not react.
  10 tries.
- Then `ad alert raise test1 C` → flashing again, caution pattern.

## Q6. Clicks (US4, SC-004) and performance (SC-006)
- Swipe 20 page changes: exactly one click each; nothing with `hapticlvl 0`.
- Operate Settings controls: one click each.
- With `stall` unacknowledged and `ad att sweep on`: `ad stat` FPS median ≥28, heap ≥40 KB.

## Q7. Strength, quiet mode, blind test (US5, SC-005)
- Low vs high: the click and caution are noticeably different.
- `ad set quiet 1`: CAUTION/ADVISORY don't vibrate, STALL does, clicks still click.
- Reboot → the settings are kept.
- **Blind test (SC-005):** the wearer, without looking, tells STALL apart from a click and a CAUTION, 3/3.
- Tune the effect ids if needed and record the final ids in `validation.md`.
