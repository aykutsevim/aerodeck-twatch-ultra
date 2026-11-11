# Quickstart & Validation: PFD Watch Face (S1)

Prerequisites: S0 quickstart prerequisites (PlatformIO via `python -m platformio`, WinLibs GCC on PATH for native
tests, watch on COM3). Serial tools must open the port with DTR/RTS low (S0 finding).

## P1. Build, test, flash
```powershell
pio test -e native                         # existing 39 + new core tests (attitude, lock, clock math, fuel)
pio run -e twatch_ultra -t upload --upload-port COM3
pio run -e twatchs3                        # must still compile
```

## P2. Set the clock (FR-010/011)
```text
ad time                                  → valid=0 on a never-set RTC; PFD shows "--" + CLOCK NOT SET
ad time set 2026-11-11T14:37:20Z          → OK; caution clears; PFD shows 17:37 local (tzoff 180)
ad reboot ; ad time                       → still correct (RTC keeps time)
ad time set 2026-11-11T20:59:55Z          → watch local 23:59:55 → 00:00:00 rollover (SC-005)
```

## P3. Attitude (US1, SC-002..SC-004)
1. Lay flat: horizon level, LOCK within 1 s (×10), no flicker for 30 s.
2. Use a phone level app strapped next to the watch: roll ±30°, pitch ±10°. Compare with `ad att` and the display (±3°).
3. `ad att` → `lat_ms`/`lat_max_ms` ≤50 ms while moving.
4. Turn the watch vertical/upside down: no spinning; full sky/ground at the scale limit.
5. `ad simfail imu` + reboot → "ATT FAIL" flag, time/EICAS still work. During the first ~3 s after boot → "ATT ALIGN".

## P4. Performance (SC-001, SC-009)
Run three times: after the US1 spike (ADI only), after US2 (with the per-frame heading tape) and on the final full PFD.
Overlay ON, move the wrist continuously for 60 s: overlay FPS ≥30 (sustained). `ad stat` heapmin ≥40 KB after 30 min
on the PFD.

## P5. Time readouts (US2, SC-005)
Compare with a reference clock for 10 min: hour box, minute box, heading = sec×6, ZULU = local − tzoff.
`ad set tzoff 330` → LOCAL shows UTC+5:30. `ad set tzoff 335` → ERR (step 30).

## P6. EICAS (US3, SC-006, SC-007)
`ad fuel` vs display: pct ±1, temp ±1 °C (×10). Plug/unplug USB ×5: CHG appears/disappears ≤2 s; FLOW shows
refuel while charging and "--" for the first 10 min on battery. LOW FUEL: verify with the battery below 15 % (or a
host test of the hysteresis, plus a serial override `ad fuel sim <pct>` if added during implementation).

## P7. Regression (FR-022)
S0 navigation (`ad nav`), caution banner, page indicator, Settings page (new "Local offset" row) all still work.
