# Validation Record: PFD Watch Face (S1)

Device: T-Watch Ultra on COM3 · Branch `002-pfd-watch-face` · Date 2026-11-11 · Host: Windows 11, PlatformIO 6.2 via
`python -m platformio`, WinLibs GCC 16.2 for native tests. Automated checks run over USB serial (port opened with
DTR/RTS low). Checks that need a hand on the watch are listed under **Pending (manual)**.

## Summary against success criteria

| SC | Criterion | Result | Evidence |
| :-- | :--- | :---: | :--- |
| SC-001 | Horizon ≥30 FPS sustained 60 s | ✅ **at threshold**: median 30, avg 29.5 (synthetic motion, overlay on, serial polling active) · ⏳ visual check with real wrist motion | §3 |
| SC-002 | Motion→display ≤50 ms | ✅ typical 24–28 ms, max 39–44 ms over 10 s windows | §2, §3 |
| SC-003 | Angles within ±3° of a phone level | ⏳ manual (needs tilting the watch) | – |
| SC-004 | LOCK within 1 s, 10/10, no flicker 30 s | ✅ flat: `lock=1` · ⏳ 10 trials by hand | §2 |
| SC-005 | Time matches a reference for 10 min incl. rollovers | ✅ rollover, reboot, offsets · ⏳ final 10-min drift reading | §4 |
| SC-006 | FUEL QTY ±1 pt, EGT ±1 °C | ✅ EICAS reads the same snapshot `ad fuel` prints (100 %, 39.8–39.9 °C) · ⏳ visual 10 samples | §5 |
| SC-007 | CHG annunciator ≤2 s, 5/5 | ⏳ manual (plug/unplug USB) | – |
| SC-008 | Hallway readability test | ⏳ manual (≥3 people) | – |
| SC-009 | Heap ≥40 KB after 30 min on the PFD | ✅ heapmin 163,260 B (60 s sweep); S0 soak 164 KB over 60 min | §3 |

## 1. Build & tests
- `pio test -e native`: **78/78** (S0 39 + S1: attitude 8, lock 5, clock_math 12, fuel 7, pfd_layout 4, settings +3).
- `pio run -e twatch_ultra` ✅, `pio run -e twatchs3` ✅ (S3: `configureAttitude` → false → ATT FAIL by design).
- Layering grep (S0 T062 rules) clean: no vendor includes outside `src/board/`, no `lv_` in core/services, core pure,
  no 502/410 literals.
- Fonts: DSEG7 Classic Bold v0.46 (zip SHA-256 `a6c2f435…c238c`), `lv_font_conv@1.5.3`, compiled into LVGL 9.6 cleanly.

## 2. Attitude (US1)
- BHI260AP gravity virtual sensor configured after the deferred probe: `status=OK rate_hz=100–108`.
- Flat on the table: raw g = (−0.001, −0.03, 0.999), pitch 1.7–1.9°, roll 0.0–1.0°, **LOCK=1**.
- `ad simfail imu` + reboot → caution `IMU FAIL`, `[att] status FAIL`, PFD shows ATT FAIL with a level picture,
  time/EICAS keep working. Normal boot → `status OK` ~3 s after Home (ATT ALIGN in between).
- Bug found and fixed: the sample watchdog compared `now − lastSample` unsigned. The svc task's `now` can be older
  than the UI task's last sample, which caused a false "no samples" FAIL/OK flapping. It now uses a signed comparison.
- **Axis signs (T014) still need a hands-on check:** the mapping is identity; flat is verified, roll/pitch signs are not.

## 3. Performance (SC-001, SC-002, SC-009): FPS gate T016/T028
Synthetic motion via `ad att sweep on` (roll ±30° @0.5 Hz, pitch ±10° @0.3 Hz), overlay FPS sampled for 60 s.

| Step | Result |
| :--- | :--- |
| T016 spike: ADI only (280 px) | **32–34 FPS** ✅, lat 27/39 ms |
| T028: full PFD, ADI 280 + heading every frame | **22–25 FPS ❌** |
| Element-cost matrix (heading still every frame) | ladder −5, labels −2, ground −2; backgrounds only = 34 (pipeline ceiling) |
| ADI capped at 238 px | 26 FPS ❌ |
| **Product-owner decision:** 238 px ADI + heading tape in 1° steps | median **30**, avg 29.4, min 28 |
| + pitch ladder trimmed ±25° → ±20° (final) | median **30**, avg 29.5. Isolated dips (10–14) coincide with serial polling |
| Refresh 10 ms instead of 16 ms | no gain, reverted |

Latency in the final configuration: `lat_ms` 28, `lat_max_ms` 42–44. Heap min 163,260 B. Details: ADR-011.

## 4. Time (US2)
- Settings migrated to schema 2: `tzoff=180` present, S0 values kept.
- `ad set tzoff 335` / `900` → `ERR out of range [-720..840 step 30]` (complete on direct re-test 4/4; an earlier
  truncated print was the validation script's parsing).
- RTC initially `2026-07-29T04:08Z integrity=0` → `valid=0`, CLOCK NOT SET raised, PFD shows `--`.
- `ad time set 2026-02-30T00:00:00Z` → `ERR bad time`.
- `ad time set 2026-11-11T20:59:55Z` → after 6 s `utc 21:00:01 local 00:00:01` (rollover at UTC+3 ✅).
- Reboot → `RTC 2026-11-11T21:00:06Z integrity=1 -> valid` ✅.
- `tzoff 330` → local 02:30 for 21:00Z ✅. `tzoff -300` → 16:00 ✅.
- Clock then set to real UTC from the PC (09:24:35Z; watch ~2 s behind the PC at set time because of command
  latency). Readings 6 min later (3 samples): watch −1.5 … −1.8 s vs the PC (whole-second resolution). **No
  measurable drift**: the offset is the set-time offset. The full 10-min reference comparison is pending.

## 5. EICAS (US3)
- `ad fuel`: `pct=100 chg=0 temp_c=39.8 flow_pph=-- valid=1` (on USB, battery full, not charging).
- `ad fuel sim 14` → `low=1`, caution LOW FUEL. `16` → still low (hysteresis). `18` → clear. `10` → LOW FUEL again.
  `ad fuel sim 101` → `ERR expected 0..100|off`.
- FUEL FLOW shows `--` until 10 min of discharge history exist (unit-tested; on-device needs ≥10 min on battery).

## 6. Regression (FR-022)
- `ad nav` ring PFD → A → B → Settings → PFD and button home/back: correct. Transitions 223–307 ms (S0 recorded
  FPS 11–22 for transitions; the PFD is a heavier page).
- `ad periph`: all OK, SD ABSENT (no card). Boot → Home 3,557 ms (≤4 s ✅).

## Findings
0. **Screen orientation (reported by the user, fixed):** the Ultra runs in portrait, **410 wide × 502 tall**
   (LilyGoLib rotation 0). `BoardUltra::display()` returned the variant's pre-rotation `DISP_WIDTH/DISP_HEIGHT`
   (502×410), so the PFD layout extended 92 px past the right edge. The fix:
   - `display()` now reports `instance.width()/height()`; `ad info` shows `display=410x502`;
   - `core::PfdLayout` gained a portrait arrangement: tapes beside a 238 px ADI, EICAS and the ZULU/LOCAL row
     stacked below, content starting below the caution banner. Host tests cover 410×502, 502×410 and 240×240.
   Re-measured: FPS median 30 / avg 29.5, latency 31/46 ms, boot 3.61 s.
   **Confirmed by the user on the watch: the portrait PFD looks right, with nothing cut off (T036/T037).**
0b. **Rounded screen corners (reported by the user, handled):** the Ultra's AMOLED corners are rounded with
   r = 64 px. `DisplayInfo` now carries `cornerR` (Ultra 64, S3 0; `ad info` → `display=410x502 corner_r=64`).
   `PfdLayout::compute(w, h, cornerR)` keeps every box inside the visible area (new host tests:
   `test_ultra_inside_rounded_corners`, `test_inside_rounded_helper`; 82/82 pass). The diagnostics overlay moved
   from the bottom-left corner to the bottom centre. Documented in the SRS (§1.3, NFR-MNT-002), the S1 spec
   (FR-018), the data model, the SAS §8, the README and constitution 1.0.2.
1. The display pipeline ceiling (~34 FPS for a 280 px redraw area) is the binding constraint for the PFD (ADR-011).
2. The sample-age race (fixed, §2).
3. The heading tape alone renders at 62 FPS. The cost appears only when combined with the ADI (two dirty areas per frame).

## Pending (manual, needs the user)
- T014: tilt right ~30° and nose-up ~10°; confirm `ad att` shows roll ≈ +30 and pitch ≈ +10 and the horizon moves the
  right way (if not, flip the sign in `kAxes`).
- T018: phone-level angle comparison (SC-003), LOCK 10 trials (SC-004), vertical/upside-down behaviour, visual FPS
  with real wrist motion.
- T034: **user observation:** unplugged → FUEL FLOW "--", plugged → REFUEL ✅. "--" on battery is the specified
  behaviour (FR-013: a drain rate needs ≥10 min of on-battery history; plugging in resets it). Still to check:
  the CHG annunciator appears/disappears within 2 s (×5, SC-007), and FLOW shows a %/h value after ≥10 min
  unplugged.
- T043: SC-008 hallway test, final 10-min clock drift reading, full quickstart pass.
