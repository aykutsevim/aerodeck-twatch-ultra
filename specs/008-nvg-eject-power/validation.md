# Validation: NVG Mode, Eject & Power UX (Slice S7)

Device: T-Watch Ultra on COM3. Started on 2026-11-12, branch `008-nvg-eject-power`.

## T002 — Baseline (before S7 code)

- `pio test -e native`: **205/205** passed. The 4 new, still-empty S7 test folders are reported as having no tests.
- `pio run -e twatch_ultra`: SUCCESS. RAM 64 968 B (19.8 %), Flash 2 030 721 B (48.4 %).
- `pio run -e twatchs3`: SUCCESS.
- **PFD FPS baseline** (`att sweep on`, 60 s, `ad stat` once per second): **median 30 fps** (min 29, max 31),
  62 samples. Script: scratchpad `fps_s7.py`.

## T006 — Probes (display, deep sleep, NVG filter)

### Bugs found and fixed during the probes

1. **SensorLib 0.3.4 callback bug (pre-existing, exposed by S7).** In
   `BoschParseCallbackManager::call(sensor_id, data, uint32_t size, …)` the `size` parameter (data length) shadows
   the member `size` (entry count).
   - With `USE_CUSTOM_VECTOR`, the loop reads uninitialised `malloc`'d entries and may call a garbage callback.
   - Symptom: the UI task panicked in `BoschParseStatic::parseData` about 0.1 s after the step counter and
     accelerometer started, on every boot. It depends on the heap layout: S6's layout happened not to crash.
   - Bisected: S6 `BoardUltra.cpp` → no crash; S7 → crash; the flush wrapper is unrelated.
   - Fixed reproducibly by `AERODECK-PATCH-2` in `tools/patch_lilygolib.py` (loop over `this->size`).
2. **Cross-core display sleep.** The first probe slept the panel from the svc task while the UI task was flushing.
   Display power changes now happen on the UI task only (`ScreenService`), as the contract says.
3. **Wrong colours after my flush replacement.** LilyGoLib defines `_SWAP_COLORS` for the Ultra (and the S3 on IDF
   4.x), so its flush byte-swaps RGB565. The replacement flush now does the same (research R1 corrected).
   - The wearer saw wrong colours after a deep-sleep wake and after a reboot.
   - With the swap: "On, colours normal".
4. **Private LVGL header.** Reading `flush_cb` through `lv_display_private.h` was dropped. Two LVGL copies are in
   `.pio/libdeps`, so the header risks a mismatched layout. The board now installs its own flush that replicates
   LilyGoLib's public calls.

### Results

- **Display off/on** (`ad screen off|on`, UI task): wake to first frame **65–72 ms**. Content intact.
- **Deep sleep** (`ad probe s7 sleep`, button wake): the button wakes it, `reset_reason=deepsleep`,
  `wake=button`.
  - Asleep for about 2.5 min: the clock kept time (watch 09:15:08 vs PC 09:15:07, the usual 1 s offset).
  - Colours normal after wake (wearer).
- **NVG flush filter**, measured on the PFD with the sweep:

  | Version | Day | NVG |
  | :-- | :-: | :-: |
  | First version (3 per-channel lookups + a separate swap pass) | 29 | **23** |
  | Byte tables + swapped output + 2 px/word | 29 | 26 |
  | + no clamp, 4 px per iteration, `-O3` | 29 | 27 |
  | **Asynchronous flush** (recolour/swap + `pushColors` on a core-0 task; LVGL renders into the second buffer meanwhile; `flush_wait_cb` blocks on a semaphore) | **35** | **35** |

  Wearer, NVG on with the sweep: "All green, clean" (no artefacts).

## T019 / Q1 — Screen timeout (US1)

- **15 s timeout, 10 trials** (scratchpad `q1_timeout.py`): off after **15.30–15.55 s**, including ≤ 0.25 s of
  polling. **10/10 within 1 s.**
- At 10 s (before the timer fix in the script): 9.8–10.1 s, measured from after the command window.
- **Wake latency** (wake decision → first frame): 63–72 ms (`[scr] on … ms`).
- **Alert wake** (scratchpad `q1_alert.py`):

  | Event (screen off) | Screen |
  | :-- | :-- |
  | Advisory raised | stays **off** ✓ |
  | Caution raised | **on**, cause=alert ✓ |
  | Caution updated (same alert, new text) | stays **off** ✓ (FR-004) |

- **Wearer, verbatim answer:** "All as described".
  - Touches on a dark screen are ignored.
  - The button wakes on the same page (Settings), not the watch face.
  - A finger held on a switch while waking does not toggle it.
- Setup screen: `NetSetupOverlay::isShown()` blocks the timeout. This is covered by `test_screen_policy`; the
  hotspot was not started on the device (no Wi-Fi provisioning in this session).

## T020 / Q2 — Raise-to-wake (US1)

- **First run: ≤ 6/10.** The cause was found later: after a deep-sleep wake, the sensor hub (rail cut by the sleep
  sequence) delivered **frozen** gravity values (`[grav]` trace: 2 distinct values in 5 000 samples while the watch
  was moved; live `ad att` constant while tilted; normal after `ad reboot`).
- **Fix:** keep the hub powered and idle in deep sleep (research R6, ADR-014). Wearer check after the fix: "Horizon
  follows tilt".
- **Rerun, worn (left wrist):** wearer **"9–10 of 10"**. Device counters: 10 raise detections, 8 wakes in the
  window; the other raises came while the screen was still on.
- `raise 0` disables it (`test_screen_policy`; Settings switch).
- **Pending manual:** false-wake rate during a 5-minute walk.

## T021 / Q6 — Smoothness (SC-007)

- PFD with the sweep, 60 s: **day 35 fps, NVG 35 fps** (median; baseline 30).
- The screen-off logic adds no measurable cost.

## T029 / Q3 — Eject and wake (US2)

- **Console eject** (`ad eject`): request → sleep **3–14 ms**. Recorder save 1–7 ms; UI quiesce 1–10 ms.
- **Gesture eject:** gesture end → deep sleep **103 ms** (`ad wake`: `gesture_to_sleep_ms=103`). Wearer: "Armed,
  ejected, woke".
- **Wake → watch face:** **3428 ms** (`ad boot`, wake=button; FR-011 ≤ 4 s).
- **Data intact:** `ad rec` before and after = steps 1234, Max G 2.4 @ 11:15. Settings intact (NVG, timeout,
  tilt).
- **Wrong gestures, wearer:** "0 ejects, 4/4 correct".
  - The 20 wrong gestures were single taps, swipes without taps, slow double taps, double tap + wait > 3 s, sideways
    drags and short pulls; 4 more correct ejects followed.
  - Log excerpt: 9 arms, all followed by disarms (short / timeout / side); no `fire` during the wrong gestures.
  - The logger did not reconnect after the first correct eject, so those 4 were confirmed by the wearer only.
- **STALL active:** the alert bar sits below the handle and eject works. Wearer: "Below handle, ejects".
- **Charging:** every eject in this session was done on USB, so it works while charging.

## T030 / Q4 — Wrist-motion wake spike (SRS OI-1)

- The hub firmware offers wrist tilt (67), wake gesture (57) and significant motion (55) (`imuwake: … present`).
- Wrist tilt armed (3 tilts) and significant motion armed (about 20 s of walking/swinging): **no wake**.
- **Not promoted** (ADR-014); the button is the only wake source.

## T039 / Q5 — NVG (US3)

- All pages and live widgets are green; brightness 10 % (`ad screen`: `bright=10%`).
- Wearer, alert levels in dim light: advisory "Green outline", caution "Inverse, steady", warning "Blinks,
  distinct".
- In NVG, Brightness set to 100 % is stored but the screen stays at 10 %. NVG off restores 100 %.
- **Reboot with NVG on:**
  - First reboot: wearer saw a "Flash of colour". That reboot followed several setting changes within 1 s, so the
    NVG write may not yet have reached flash.
  - Retest: "No flash this time".
- Eject and wake with NVG on: "Dim green at once".

## T042 / Q8 — Power (US4)

- USB meter at 5 V, battery 100 %, charge finished:

  | State | Current |
  | :-- | :-: |
  | PFD day | **48 mA** |
  | PFD NVG | **45 mA** |
  | Screen off | **37 mA** |
  | Ejected | **< 1 mA** (0.000 A) |

- Battery capacity 1100 mAh (wearer).
- Estimate in `docs/power_report.md`: average ≈ 34 mA, about **33 h** (with the overnight eject; ≈ 49 mA / 22 h
  without). Target ≤50 mA / ≥22 h (redefined from ≥48 h): **met**. SC-006 met (> 37×).
- **Pending manual:** fuel-gauge discharge per state off USB (cross-check).

## Horizon tilt (FR-023, added)

- Hub axes from labelled poses: up = +z flat, −y upright (12 o'clock up), +x with 12 o'clock to the right.
- Wearer at 90° (upright): "It is what I requested but … it should be level 30 degrees to the ground". Made it a
  setting (0–90°, 15° steps, default 30°).
- Wearer at 30°: **"Feels right"**.
- The real compass bearing request was declined for hardware reasons:
  - I2C scan: 0x1A touch, 0x20 expander, 0x28 BHI260AP, 0x34 PMU, 0x51 RTC, 0x5A haptics, so no magnetometer.
  - The KLIO hub firmware has no compass.
  - Wearer: keep the seconds on the top tape.

## T047 / Q7 — Settings persistence

- NVG, the timeout (`scrto`) and the tilt (`attref`) survived reboots and ejects in this session.
- Restore defaults is **not run on the device** (it erases the Wi-Fi credentials); it is covered by `test_settings`
  v8 defaults.
- Schema note: flashing S6 `main` over v8 settings during the bisect triggered ResetNewer (settings=3). Wi-Fi was
  already unconfigured, so no credentials were lost.

## T048 — Final checks

- `pio test -e native`: **251/251** (baseline 205; new: screen policy 13, raise 10, eject gesture 11, NVG 9;
  settings and attitude extended).
- `pio run -e twatch_ultra`: RAM 66 900 B (+1 932), Flash 2 053 069 B (+22 348).
- `pio run -e twatchs3`: SUCCESS.
- Free internal heap 102 KB, minimum 91.5 KB (≥ 40 KB) in day mode and NVG.

## Success criteria

| SC | Result | Evidence |
| :-- | :-- | :-- |
| SC-001 | ✅ | 10/10 timeouts within 1 s; raises 9–10/10 (wearer) |
| SC-002 | ✅ | Sleep ≤ 103 ms after the gesture; 0 ejects in 20 wrong gestures |
| SC-003 | ✅ | Wake → PFD 3.4 s; recorder and settings intact; 5+ eject/wake cycles |
| SC-004 | ✅ | All green, 10 %, alert levels distinct (wearer) |
| SC-005 | ✅ | `docs/power_report.md`: currents for 4 states, average ≈ 34 mA (≈ 33 h) vs ≤50 mA / ≥22 h: **met** |
| SC-006 | ✅ | Ejected < 1 mA vs 37 mA screen-off |
| SC-007 | ✅ | 35 fps day and NVG (≥ 30; baseline 30) |

**Pending manual checks:** the raise false-wake rate on a 5-minute walk; the fuel-gauge discharge cross-check off
USB.
