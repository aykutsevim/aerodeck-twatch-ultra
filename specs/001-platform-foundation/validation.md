# Validation Record: Platform Foundation (S0)

Device: LilyGO T-Watch Ultra on COM3 · Firmware: branch `001-platform-foundation` (`AD_FW_VERSION` from
`git describe`) · Date: 2026-11-11 · Host: Windows 11, PlatformIO Core 6.2.0 run via `python -m platformio`.

Automated checks were run over USB serial using the `ad` console, plus the `ad nav` test hook for navigation.
Checks that need a finger on the watch or a hand on the power supply are listed under **Pending (manual)**.

## Summary against success criteria

| SC | Criterion | Result | Evidence |
| :-- | :--- | :---: | :--- |
| SC-001 | Home interactive ≤4 s (amended from 3 s), 10/10 cold boots | ✅ 10/10 at 3.45–3.51 s | §1 |
| SC-002 | 100 swipes land on the expected page | ✅ 100/100 (via `ad nav`) · ⏳ physical swipe check | §2 |
| SC-003 | Button → Home ≤0.5 s, 20/20 | ✅ 20/20, max 247 ms (via `ad nav button`) · ⏳ physical button | §2 |
| SC-004 | Settings kept across 10 power cycles incl. 3 cut during save | ✅ 10/10 reboots · ⏳ 3 power-cut trials | §3 |
| SC-005 | Each optional failure → Home + named caution; 0 resets in 1 h | ✅ all 8 + all-at-once · ✅ 60-min soak, no reset | §4, §7 |
| SC-006 | Transitions ≤0.3 s; FPS recorded (amended; ≥30 FPS moved to S1) | ✅ max 270 ms · FPS 15–28 recorded | §2, §5 |
| SC-007 | Heap ≥40 KB after 30 min navigation | ✅ heapmin 164,172 B over 60 min | §7 |
| SC-008 | Fresh clone builds Ultra with one command | ✅ `patched` then `already patched` | §6 |
| SC-009 | S3 compiles; native tests pass | ✅ S3 · ✅ 5/5 suites, 39/39 tests | §6 |

## 1. Boot time (V2, SC-001)

10 cold resets (RTS), `boot->home` = `esp_timer_get_time()` at Home's first `SCREEN_LOADED`:
`3495 ×10` ms. With simulated failures: 3446–3454 ms.

Breakdown from a debug build (`CORE_DEBUG_LEVEL=4`, with timestamps):
- ~1.2 s ROM bootloader + PSRAM init before `setup()`.
- LilyGoLib `begin()`:
  - 700 ms of fixed delays (200+300+200) in the display reset sequence.
  - ~760 ms display panel init.
  - ~620 ms switching to the 32 kHz crystal (`esp_enable_slow_crystal`).
  - Remaining PMU, expander, DRV2605 and touch init.
- The BHI260AP firmware upload (~2.1 s) and the GPS/NFC/LoRa/SD probes were **moved out of the boot path**
  into a background probe after Home is shown (`IBoard::probeDeferred`, ~3.0 s). This cut boot from
  5.66 s to 3.50 s.

**Conclusion:** without patching more LilyGoLib internals (display reset delays, crystal switch), the floor
is about 3.3–3.5 s. This needs a decision (see "Open decisions").

## 2. Navigation (V3)

Using the `ad nav` hook (same code path as swipe/button after gesture recognition):
- Ring order and wrap: 100/100 random swipes (50 next, 50 prev) landed on the expected page.
- Swipe transition time: avg 219 ms, max 270 ms (≤300 ✅).
- Button → Home from random pages: 20/20, avg 224 ms, max 247 ms (≤500 ✅). Button on Home returns to
  the last page (exercised in the same run). Button during a transition ends on Home (unit-tested in
  `test_nav`).

## 3. Settings (V4)

- `ad set bright 0`, `overlay 1` → kept across **10/10** reboots.
- `ad set bright 9` → `ERR out of range [0..4]`. `ad set simfail 1` → `ERR not editable`.
  `ad set nope 1` → `ERR no such key`.
- `ad set reset` → `defaults restored`. After reboot: `bright=3 overlay=0 loglvl=2 simfail=0`.
- First boot on a blank NVS logged `first boot: writing defaults`.

## 4. Graceful degradation (V5, V7)

`ad simfail <x>` + reboot, for each peripheral:

| Simulated | Home reached | Cautions raised |
| :--- | :--- | :--- |
| lora | 3454 ms | LORA FAIL |
| nfc | 3454 ms | NFC FAIL |
| gnss | 3454 ms | GNSS FAIL |
| sd | 3454 ms | SD FAIL |
| mic | 3452 ms | MIC FAIL, SPEAKER FAIL |
| haptics | 3446 ms | HAPTICS FAIL |
| imu | 3454 ms | IMU FAIL |
| rtc | 3447 ms | RTC FAIL |
| all 8 | 3448 ms | all 9 (RTC, IMU, HAPTICS, LORA, NFC, GNSS, SD, MIC, SPEAKER), `ad periph` = SIMFAIL |
| none | n/a | no cautions; SD = ABSENT (no card inserted; detected through the expander card-detect pin) |

Boot-loop protection (T047): a build with `-DAD_TEST_DISPLAY_CRASH` aborted 3× at the DisplayInit stage, then
booted **headless** (`mode=2 fastFails=3`). It logged FATAL every 5 s, `ad info` answered
(`reset_reason=panic safe_mode=1`) and uptime kept increasing with no further resets. The flag was removed
afterwards; the normal build is flashed.

Rails (V7): the boot log shows `rail lora/audioamp/gnss/nfc OFF (boot)`, then `idle rails enforced OFF` after the
deferred probe.

✅ 1-hour run with `simfail lora`: see §7.

## 5. Diagnostics & performance (V6, SC-006, SC-007)

- `ad log warn` → 0 INFO lines on the next boot. `ad log info` → INFO lines resume.
- `ad stat` idle: `heap≈171 KB heapmin≈164 KB psram≈8.3 MB`.
- FPS during transitions, measured as frames rendered between the navigation request and `SCREEN_LOADED`:

| Configuration | Simple pages | Settings page | Button transition |
| :--- | :--- | :--- | :--- |
| DMA partial (1/6-screen internal buffers), refr 33 ms, fade | 24–26 | 14–17 | 11–12 (fade) |
| **DMA partial, refr 16 ms, slide (current)** | **27–28** | **15–17** | **19** |
| PSRAM full-frame, no DMA | 6–7 | 5–6 | 6 |

The current configuration is the fastest measured, but it is **below 30 FPS** (render/flush bound at 502×410).
The Settings page (default-theme widgets) is the slowest. See "Open decisions".

## 6. Builds

- Clean checkout (working tree copied without `.pio`, fresh PlatformIO workspace): `pio run -e twatch_ultra`
  → `patch_lilygolib: patched (twatch_ultra)` SUCCESS. Second run → `already patched` SUCCESS.
- `pio run -e twatchs3` (fresh libdeps) → `patched (twatchs3)` SUCCESS.
- Dependency graph (T005) linked only the pinned versions: LilyGoLib 0.2.0, lvgl 9.6.0, RadioLib 7.8.1,
  XPowersLib 0.3.3, SensorLib 0.3.4, TinyGPSPlus 1.1.0, ESP8266Audio 2.0.0, NimBLE 2.5.2, the BLE forks @ pinned SHAs,
  ST25R3916/NFC-RFAL @ pinned SHAs. LilyGoLib's own older transitive copies (lvgl 9.2.2, RadioLib 7.1.2,
  SensorLib 0.3.1, XPowersLib 0.2.9) are installed but **not linked**.
- Native tests (`pio test -e native`, WinLibs GCC 16.2 UCRT installed via winget): **39/39 passed**:
  `test_boot_guard` 7, `test_command_parser` 10, `test_log_format` 5, `test_nav` 9, `test_settings` 8.
  The first run had `test_nav` blocked by Windows Smart App Control (`WinError 4551`). It passed after the user
  disabled Smart App Control.
- The WinLibs `bin` directory was added to the **user PATH** by the installer. New terminals see `gcc`;
  sessions already open need restarting.
- Windows note: building from a very deep directory fails on RadioLib's long example paths (MAX_PATH). Added
  to the README.

## 7. Soak (SC-005 1 h, SC-007 30 min)

60 min of continuous `ad nav` (8,877 navigations) with `simfail lora`, sampling `ad stat`/`ad info` every 2 s
(1,651 samples):
- **No reset:** uptime went from 10 s to 3,609 s, matching the 60 min elapsed. The script's "resets detected: 5"
  counter is a parsing artefact: replies were sometimes cut at the 0.3 s read window, so a partial number such as
  `uptime_s=3` compared lower than the previous reading.
- **Heap:** lowest-ever free internal heap 164,172 B (SC-007 ≥40,960 ✅).
- **FPS** (1 s windows during constant navigation): min 2, median 20, max 29 (recorded per amended SC-006).

## Findings fixed during validation

1. Arduino's `bit(b)` macro shadowed a lambda named `bit` in `periph()`, so peripherals were misreported as
   FAILED/OK by bit value. Renamed to `has`.
2. LilyGoLib powers the LoRa/GPS/NFC rails only in `initPMU()`. The deferred probe powers each rail briefly,
   then PowerService turns it off again.
3. `ad simfail` with all 8 names exceeded the 8-token parser limit. Raised to 12, with a unit test added.
4. Serial replies lost lines. Fixed with a TX timeout plus a flush after each reply line. In addition, opening the
   port with DTR/RTS asserted resets the ESP32-S3; host tools must open with DTR/RTS low.
5. AXP2101 has **no battery-current ADC**: `PmuReading.battmA` reports "unavailable". This matters for S1's
   FUEL FLOW gauge (SRS FR-PFD-007).

## Pending (manual, needs the user)

- Real swipe gestures on the touch panel, including vertical swipes being ignored, and the page indicator visible ~1 s (US1).
- Physical side-button presses (US1).
- On-watch Settings: brightness −/+ visibly changes the screen; overlay switch shows FPS/heap; restore-defaults
  dialog Cancel/Confirm (US2).
- 3 trials of cutting power right after changing brightness (SC-004).
- Caution banner visible and cycling (US3).

## Decisions taken (2026-11-11, by the product owner)

- **SC-001 / FR-001 / SRS FR-PLT-001:** boot target amended from 3 s to **4 s** (measured floor ~3.45 s).
- **SC-006:** relaxed to transitions ≤300 ms with FPS *recorded*. The ≥30 FPS requirement stays with the PFD
  horizon in S1 (NFR-PERF-001), which also owns rendering tuning (SRS OI-2 updated).
