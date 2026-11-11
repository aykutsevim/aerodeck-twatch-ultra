# S4 Validation: Alert Manager & Haptics

**Device:** T-Watch Ultra (COM3), branch `005-alert-haptics` · **Date:** 2026-11-11
**Script:** `validate_s4.py` (session scratchpad), plus `validate_s1.py` for regression.

## Summary

| Criterion | Result | Evidence |
|---|---|---|
| SC-001 battery sweep 3/3 | ✅ 3/3 | §Q4 |
| SC-002 WARNING first 10/10 | ✅ 10/10 | §Q3 (device) + test_alerts (host) |
| SC-003 ack stops vibration ≤1 s | ✅ serial · ✅ user ("all ok") | `ad alert ack` → no shaker afterwards, `stop shaker` logged at once |
| SC-004 one click per page change | ✅ log · ✅ user ("all ok") | 20/20 clicks logged, latency max 3 ms (avg 1.9 ms); 0 with `hapticlvl 0` |
| SC-005 blind test | ✅ user ("all ok") | not logged |
| SC-006 PFD ≥28 fps with a flashing WARNING | ✅ | median 29 (min 21), heapmin 57.0 KB |

## Phase 2 / regression
- `pio test -e native`: **161/161** (+ test_alerts 9, test_battery_alarm 5, test_haptic_policy 4, settings v5).
- `pio run -e twatch_ultra` and `pio run -e twatchs3`: SUCCESS.
- Layering:
  - DRV2605 / `instance.drv` only in `src/board/`;
  - no `lv_` in services;
  - `Cautions.{h,cpp}` and `CautionBanner.{h,cpp}` removed; no references remain (only the function name
    `raisePeriphCautions` and a comment).
- S1 regression:
  - PFD median 30 fps (min 28);
  - attitude OK; the IMU simfail now raises `CAUTION IMU FAIL id=4` through AlertManager;
  - nav ring PFD → WX → PAGE B → SETTINGS;
  - all peripherals OK; boot→home 3.72 s.
- S2/S3: boot sync and weather fetch Ok on every boot during these tests.
- `SvcTask::kMaxTicks` was raised 8 → 12: AlertManager's tick makes 8, and S5 needs headroom.

## Q2: migration and retrofit
- Settings v5: `hapticlvl=1 quiet=0` were added; the S0–S3 values are unchanged.
- IMU simfail → `CAUTION IMU FAIL` (band amber).
- `ad wifi age 90000` → `ADVISORY TIME UNSYNC`; cleared by the next sync.
- CLOCK NOT SET: the S1 TimeService raises it only at boot (an invalid RTC), not on `ad time set` with an implausible
  year. This behaviour is unchanged by S4; the retrofit only switched the call to AlertManager. Code-reviewed.

## Q3: ordering
- 10 random orders of {test1 A, test2 C, clocknotset C, stall W, test3 C}: STALL first and levels in order **10/10**.
- `ad alert ack` → `acked=5`, then `unacked=0`. Clearing all → `n=0`, band hidden.

## Q4: battery sweep (SC-001)
`ad fuel sim <p> nochg`, 20 → 3 → 20 at 1 step/1.15 s, three runs. All runs gave exactly:
`STALL` (14 %) → `clear STALL` + `PULL UP` (5 %) → `clear PULL UP` + `STALL (acked)` (7 %) → `clear STALL` (17 %).
No flicker at 15/16 or 5/6. `chg` at 10 % → STALL cleared within one poll; nothing active.

**Boot with low battery:** STALL was first raised at 3.64 s, *before* the face (3.72 s). Fixed:
`PowerService::enableBatteryAlarm()` is now called from `onFirstHome`. Re-test: face at 3.68 s, STALL at 6.12 s
(after the face).

## Q5: acknowledge (serial part)
- `ad alert raise stall` → the shaker played 3× in 5 s, intervals **1997 / 2001 ms**.
- `ad alert ack` → `stop shaker` logged, then **0** shaker plays in the next 4 s.
- A CAUTION raised after the ack → `raise CAUTION`, and the caution pattern played (re-armed).

## Q6: clicks and performance
- 20 page changes (`ad nav next`): **20 clicks**, latency (nav settled → `[hap] play click`) max **3 ms**, avg 1.9 ms
  (FR-010 ≤50 ms). `hapticlvl 0` → 0 clicks.
- PFD with an unacknowledged STALL (flashing band + shaker) and the sweep: FPS median **29**, min 21; heapmin 57.0 KB.

## Wearer checks (T013, T017, T019, T021, T024, T026)
- 2026-11-11: the user reported **"all ok"** for the wearer checks (pattern feel at low/high, band look, STALL with
  vibration off, tap to acknowledge, clicks on swipes, quiet mode, blind distinction).
- The guided script (`haptic_tour.py`) was **not** run, so there are no logged tap/swipe counts from the wearer. The
  serial-path measurements above stand.
- The effect ids stay as planned (research R2): click 5/4, advisory 8/7, caution 11/12, shaker 52 x3. The nominal
  lengths are not GO-bit measured; they are the planned values (30/100/400/1500 ms).

## Originally pending (now confirmed by the user)
- **T013:** feel each pattern (click / advisory / caution / shaker) at low and high; record the final effect ids.
- **T017:** the band's look on PFD / WX / Settings.
- **T019:** STALL still vibrates with `hapticlvl 0`.
- **T021:** tap the band 10× (vibration stops ≤1 s; nothing underneath reacts).
- **T024:** 20 swipes → one click each felt.
- **T026:** low vs high, quiet mode, reboot keeps settings, blind test (SC-005).
