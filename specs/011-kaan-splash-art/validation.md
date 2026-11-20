# Validation: KAAN Splash Screen and Watch-Face Emblem (feature 011)

**Device**: T-Watch Ultra (COM3). Boot timings come from `ad boot` (`boot_to_home_ms`), taken with
`boot_bench.py` (`ad reboot` / `ad probe s8 alarm 15`, then `ad boot`).

## T001 — Baseline (branch `011-kaan-splash-art`, before any code), 2026-11-17

| Item | Value |
| :-- | :-- |
| `pio test -e native` | 305/305 |
| Ultra build | RAM 74 304 B (22.7 %), flash 2 083 633 B (49.7 %) |
| S3 build | RAM 70 356 B, flash 1 917 437 B |
| Full start (`ad reboot` ×3) | `boot_to_home_ms` **3 826 / 3 826 / 3 823** (`wake=poweron`) |
| Wake from deep sleep (S8 RTC-alarm wake ×3) | `boot_to_home_ms` **3 493 / 3 493 / 3 493** (`wake=timer`) |

The wake baseline uses the RTC-alarm wake. It goes through the same deep-sleep wake path as a button wake, needs
nobody at the watch, and is deterministic. SC-002 compares against **3 493 ms**.

## T003/T004 — Assets (Q0)

`python tools/gen_kaan_art.py` (PowerShell/.NET bicubic, about 1.3 s):

| Item | Value |
| :-- | :-- |
| Crop box (content + 4 % margin) | x 208, y 368, 1304 × 1952 of 1696 × 2528 (aspect 0.668) |
| Splash | **325 × 486 A8** (158 KB); fits the 410 × 502 r = 64 screen with an 8 px margin on the first try |
| Emblem | **57 × 85 A8** (4.8 KB); the height equals the Ultra `PfdLayout` emblem box (85 px); width cap contentW/4 = 97 |
| Deterministic | two runs: identical SHA-256 of `KaanArt.cpp` |
| Preview | the whole aircraft and "KAAN" visible, centred; the corners only clip black; at emblem size the lettering is still readable |

## T009–T013, T016 — Splash on the device (US1, measured)

**Design change during implementation (research R2 revised):** the first version pushed the splash after
`instance.begin()` (touch, RTC and mic init split out). It was visible only **≈ 275 ms**: `[boot] splash shown at
3563 ms`, PFD at ≈ 3 838 ms. A debug-log build (`CORE_DEBUG_LEVEL=4`, not committed) showed where `begin()`
spends its time:
- the panel reset alone is 200 + 300 + 200 ms;
- the panel init sequence runs twice, with its 120 ms waits;
- after the panel is ready, `esp_enable_slow_crystal()` spends **≈ 0.6 s** calibrating the 32 kHz crystal before
  the touch, RTC and audio init.

LilyGoLib's boot image is pushed exactly between the panel init and that calibration, but it is followed by a
synchronous `incrementalBrightness(250, 20)` (5 s). So:
- **AERODECK-PATCH-3** (`tools/patch_lilygolib.py`, automatic and idempotent) replaces the fade with a weak hook
  `aerodeck_boot_image_shown()`;
- BoardUltra implements the hook: brightness = the user's level (10 % in NVG), plus the timestamp;
- the splash (410 × 502 RGB565, 411 KB PSRAM) is composed on core 0 during LilyGoLib's PMU and panel reset
  (57 ms, off the boot path) and registered with `setBootImage()` only when complete;
- the init split was reverted: LilyGoLib's init order is unchanged again, which removes the regression risk of
  T010.

| Full start (`ad reboot`) | `boot_to_home_ms` | `splash_at_ms` | visible |
| :-- | :-: | :-: | :-: |
| 1 (first after flashing) | 3 927 | 2 853 | 1 074 ms |
| 2 | 3 883 | 2 808 | 1 075 ms |
| 3 | 3 882 | 2 808 | 1 074 ms |
| 4 | 3 882 | 2 808 | 1 074 ms |
| 5 | 3 883 | 2 808 | 1 075 ms |

- **FR-003 / SC-001:** all ≤ 4 000 ms (+57 ms over the T001 baseline of 3 825 ms: the full-frame push and the
  brightness command). The splash is visible for **≈ 1.07 s**.
- **SC-002 (T016):** S8 RTC-alarm wake ×3: `boot_to_home_ms` **3 489 / 3 489 / 3 490**, `splash=0 skip=wake`.
  The mean of 3 489 ms is within 50 ms of the baseline (3 493 ms).
- Composition `splash_cost_ms=57` (core 0, in parallel). Flash +159 KB, RAM +40 B.

## T014 — Panic found and fixed (SPI bus-lock race)

With the boot-image splash, the **first page swipes panicked** (3 of 3 runs: PFD→WX→RECORDER, WX→PFD, CHRONO→TCAS),
also with TCAS off. The backtrace matched the S9 one-off panic: `spi_intr` → `spi_bus_lock_bg_exit` →
null read (EXCVADDR 0), interrupting `lv_timer_handler`. Bisect:

| Build | Result |
| :-- | :-- |
| no splash (`setSplash` skipped) | no panic |
| splash pushed, brightness command in the hook removed | no panic |
| splash pushed + `setBrightness()` immediately after the push | **panic, 3/3** |
| splash pushed + **2 ms wait**, then `setBrightness()` | no panic, 3/3 boot + 8-swipe runs |

Cause: `pushColors()` (queued DMA) returns before the SPI ISR has finished exiting the background transaction. A
polling panel command (`writeCommand`) issued in that window corrupts ESP-IDF 4.4's SPI bus lock, and a later DMA
flush crashes in the ISR. Fixes:
- the hook waits 2 ms before the brightness command;
- `BoardUltra::waitFlushIdle()` waits 2 ms after the flush task goes idle. It runs before every runtime polling
  panel command: brightness, screen off/on (S7) and deep sleep. This is the likely cause of S9's unexplained panic
  during wearer interaction (screen time-outs on the TCAS page).

Stress after the fix: boot, the TCAS page animating, **10 × screen off/on + 10 × NVG on/off** (brightness commands),
then a full swipe cycle: no reset (uptime 67 s). The watch then ran for **≈ 23 h** (uptime 84 036 s, TCAS beaconing)
without a reset.

## T015 — Splash, wearer (US1, SC-001, SC-003)

3 day-mode restarts + 2 in NVG (`boot_to_home_ms` 3 930 (first after flashing) / 3 888 / 3 885 / 3 885 / 3 885;
splash at 2 810–2 854 ms). Wearer:
- day: **"Looks right"**: the whole aircraft and "KAAN" visible, centred, undistorted, not cut by the corners, and
  the watch face fully replaced it (no leftovers, T014 visual part);
- NVG: **"Green, dim"**: no glare, no white flash (FR-008).

Final firmware (with the 2 ms fix), `ad reboot` ×3: `boot_to_home_ms` **3 885 / 3 886 / 3 930**, splash at 2 810 / 2 810 / 2 855 ms (visible ≈ 1.07 s), all ≤ 4 000.

## T017 — Peripheral failures (Q3, FR-005)

| Step | Result |
| :-- | :-- |
| `ad simfail rtc` + `ad reboot` | splash shown (3 881 ms); `RTC present=1 state=SIMFAIL`; CAUTIONs `RTC FAIL` + `CLOCK NOT SET` |
| `ad simfail mic` + `ad reboot` | splash shown (3 890 ms); `MIC`/`SPEAKER state=SIMFAIL`; CAUTION `SPEAKER FAIL` |
| `ad simfail none` + `ad reboot` | splash shown (3 853 ms); `ad periph` all as in T001 (SD absent, the rest OK) |

## T018/T019 — PFD emblem (US2, SC-004..006)

| Check | Result |
| :-- | :-- |
| PFD fps (`ad att sweep on`, `ad stat` ×3) | **35 / 36 / 36** (≥ 30); heap 71 936 B (≥ 40 KB) |
| Placement (wearer, NVG on) | **"Placed right"**: centred between ZULU and LOCAL, no overlap, green in NVG |
| Look (wearer) | **"Keep as is"**: recognisable, the lettering is fine at 57 × 85, soft white by day is right |

## T021 — Regression (Q5)

| Check | Result |
| :-- | :-- |
| Audio (S5) | `ad audio play traffic` → `play traffic`; wearer: **"Heard it"** |
| RTC (S2) | boot log `[time] RTC … valid`; `ad time` correct (`valid=1`, local +3 h) |
| Pages | full swipe cycle PFD→WX→RECORDER→CHRONO→TCAS→SETTINGS→PFD, no reset |
| Eject + wake + touch | the wearer's first attempt did not eject (the watch kept `wake=poweron`, uptime 23 h). The second attempt: pull-to-eject (`src=gesture`), side-button wake: **`boot_to_home_ms=3492 wake=button splash=0 skip=wake`**; swipe and tap worked at once (wearer: "Done, worked") |
| `ad periph` | identical to T001 |

## T022 — Final checks

| Item | Value |
| :-- | :-- |
| `pio test -e native` | **314/314** (baseline 305; +9: `test_splash_compose` 7, `test_pfd_layout` emblem 2) |
| Ultra build | RAM 74 376 B (+72 B), flash 2 248 317 B (**+164 684 B**: the splash 158 KB + emblem 4.8 KB A8 masks + code) |
| S3 build | SUCCESS, RAM 70 404 B (+48 B), flash 2 081 401 B (+163 964 B: the art is linked, but no splash/emblem is shown on the S3) |
| Patch 3 | applied idempotently in both envs (`boot image patched` / `already patched`) |
| Generator | re-run: `KaanArt.cpp` SHA-256 unchanged (`ED3950EB…`) |

## Success criteria

| SC | Status | Evidence |
| :-- | :-- | :-- |
| SC-001 | Met | 5 + 3 + 3 + 5 full starts, all `splash=1`, `boot_to_home_ms` 3 853–3 930 (≤ 4 000) |
| SC-002 | Met | timer wakes 3 489 / 3 489 / 3 490 ms (baseline 3 493, within 50 ms), `skip=wake`; gesture eject + button wake 3 492 ms, `skip=wake` |
| SC-003 | Met | wearer: "Looks right" (complete, undistorted, corners clear, clean switch to the PFD) |
| SC-004 | Met | wearer: "Placed right", "Keep as is" |
| SC-005 | Met | PFD 35 / 36 / 36 fps with the emblem |
| SC-006 | Met | wearer: emblem green in NVG; NVG splash "Green, dim" |

Also found and fixed: the ESP-IDF SPI bus-lock race (T014), and the likely cause of S9's open panic issue.

## T024 — KAAN page (US3, user addition)

| Check | Result |
| :-- | :-- |
| Ring | `nav PFD->KAAN` (swipe right / `prev`), `KAAN->PFD`, `KAAN->SETTINGS`, `SETTINGS->KAAN`: 7 pages, KAAN last |
| Start page | the full start lands on the PFD after the splash (`nav PFD->PFD boot`) |
| Resources | flash +300 B (the page reuses the splash art); heap 71 928 B on the KAAN page after the boot settled (same as the PFD); static page (0–2 fps, no redraws) |
| Wearer | **"Looks right"**: full KAAN art, centred, swipes work both ways |
