# Research: Alert Manager & Haptics (Slice S4)

Checked on 2026-11-11 against the installed code: LilyGoLib 0.2.0 (`LilyGoWatchUltra.cpp`, `LilyGoWatchS3.h`),
SensorLib `SensorDRV2605.hpp`, and the S0–S3 sources.

## R1 · Haptic driver access (IBoard)
- **Finding:**
  - Both boards carry a DRV2605 on the shared I2C bus. LilyGoLib initialises it in `begin()` with
    `selectLibrary(1)`, `setMode(MODE_INTTRIG)` and `useERM()`, and plays effect 15 once as a boot buzz.
  - SensorLib exposes `setWaveform(slot 0..7, w)`, where `w` is a ROM effect id or `0x80 | n` = wait n×10 ms, a
    0 ends the sequence. There are also `run()` and `stop()`.
  - Nothing in IBoard exposes it today.
- **Decision:** extend `IBoard` with:
  - `bool haptic(const uint8_t* seq, uint8_t n)`: write up to 8 sequence entries, terminate, `run()`, holding the
    I2C lock. Returns false if `Periph::Haptics` is not Ok;
  - `void hapticStop()`.

  Both are implemented in `BoardUltra` and `BoardS3`. This is the AD-SAS-001 §6 "IBoard::haptic" hook, generalised
  from a single effect id to a short sequence so the stick shaker is one hardware sequence.
- **Rationale:**
  - Constitution II: driver types stay in `src/board/`.
  - A sequence lets the chip time the pulses itself, so no task wakes every 100 ms.
- **Alternatives:**
  - RTP (real-time PWM) mode for amplitude control: needs a ~10 ms update loop and is ERM-dependent. Rejected: the
    ROM library already has 100 / 60 / 30 % variants of each effect.

## R2 · Effects and patterns (ERM, library 1)

| Pattern | High | Low | Off / quiet | Nominal length |
|---|---|---|---|---|
| Click (UI) | 4 *Sharp Click 100 %* | 5 *Sharp Click 60 %* | none (off) | ~30 ms |
| Advisory | 7 *Soft Bump 100 %* | 8 *Soft Bump 60 %* | none | ~100 ms |
| Caution | 12 *Triple Click 100 %* | 11 *Double Click 60 %* | none | ~400 ms |
| Stick shaker (WARNING) | 52 *Pulsing Strong 1 100 %* ×3 with 100 ms waits, always full strength | same | same (never off) | ~1500 ms |

- The stick shaker repeats every **2000 ms** until acknowledged (FR-012).
- The effect ids are a starting point. They are tuned by feel on the device (quickstart Q4, SC-005), and the final ids
  are recorded in `validation.md`.
- Nominal lengths are used for haptic priority (R5). They are measured once on the device by timing the driver's GO
  bit, and the table is updated.
- **Alternatives:** a custom waveform via RTP. Rejected (R1).

## R3 · Alert core (pure, host-tested)
- **Decision:** `core::AlertSet`, a fixed array of 24 entries. Entry fields: `id`, `level`, `text[25]`, `acked`,
  `raisedSeq`. Raising takes a monotonically increasing sequence number, which gives a deterministic order.
  - **Order:** level desc, then `raisedSeq` asc (FR-002).
  - **Operations:**
    - `raise(id, level, text)` → `Added` / `Updated` (text or level changed) / `Same`;
    - `clear(id)`;
    - `ackAll()`;
    - `master()`;
    - `sorted(out)`;
    - `anyUnacked()`.
  - **Re-arming:** an `Updated` with a higher level is unacknowledged again. `Same` does not replay a pattern.
- `core::AlertTable` (constexpr): `AlertId → {level, pattern, repeatMs, voiceClip(0 = none, S5)}`.
  - The ids keep the S0 `CautionId` values (PeriphFirst + n, SettingsReset 32, …, TimeUnsync).
  - New ids: `Stall`, `PullUp`, and `Test1..Test3` (the diagnostic `ad alert raise`).
- `core::BatteryAlarm`: states `Normal → Stall → PullUp`.
  - `Stall` when pct < 15 and not charging; `PullUp` when pct ≤ 5 and not charging.
  - Recovery: `PullUp → Stall` when pct ≥ 7 (STALL raised **already acknowledged**: de-escalation does not nag);
    `Stall → Normal` when pct ≥ 17; charging → `Normal` from any state.
  - Invalid readings are ignored.
- `core::HapticPolicy`: `select(pattern, intensity, quiet) → effect sequence | none`.
  - WARNING always plays at full strength.
  - CAUTION/ADVISORY: none when quiet or off.
  - Click: none when off (quiet doesn't affect clicks).
  - `shouldPlay(newPrio, curPrio, curEndMs, nowMs)`: a lower priority never pre-empts a higher one that is still
    playing; equal or higher replaces it.
- `core::RepeatClock`: WARNING repeat timing (every 2000 ms while `anyUnackedWarning`).

## R4 · Services and tasks
- `AlertManager` (svc task tick, core 0):
  - owns the `AlertSet` behind a lock and publishes `Snapshot<AlertSnapshot>` (sorted list, up to 16, plus counts
    and `anyUnacked`);
  - posts the `AlertRaised` / `AlertCleared` / `AlertAcked` events (AD-SAS-001 §5.2 names), which replace
    `CautionChanged`;
  - on `Added`/re-armed, asks `HapticService` to play the pattern; while an unacknowledged WARNING exists, re-plays
    the stick shaker every 2000 ms;
  - `raise()` and `clear()` are callable from any task (they take the lock; haptics are triggered from the svc tick
    to keep I2C off foreign tasks).
- `HapticService`: `play(pattern, prio)` and `click()`, callable from any task. A small mutex guards the "current
  priority / end time" state. Calls `board().haptic()`, which takes the I2C lock.
  - **Click latency:** `click()` from the UI task runs immediately: 4 register writes at 400 kHz ≈ 0.5 ms, well
    within 50 ms (FR-010).
  - **No queue:** a click replaces a pending click and is dropped during a WARNING (FR-011).
- `PowerService`: replaces the S1 LOW FUEL caution logic with `core::BatteryAlarm` → `AlertManager::raise/clear(Stall
  /PullUp)`. The EICAS fuel gauge colours (`FuelState`) are unchanged.
- **Retrofit:** `Cautions::raise/clear` call sites (main.cpp peripheral failures, settings reset/storage, safe mode;
  TimeService CLOCK NOT SET; NetService TIME UNSYNC; PowerService LOW FUEL) move to `AlertManager`. `Cautions.h/.cpp`
  and `CautionBanner` are removed (constitution VIII: one alert path).

## R5 · Annunciator band (UI)
- **Decision:** `ui/AnnunciatorBar`, which replaces `CautionBanner` on `lv_layer_top()`:
  - full width, height **S(28)** (the same as the S0 banner, which the PFD layout and the WX page already reserve),
    top;
  - text centred: master message + `  +N`;
  - background in the level colour: WARNING `Theme::warning()` red with white text; CAUTION `Theme::caution()` amber
    with black text; ADVISORY `Theme::advisory()` cyan with black text.
- **Flashing:** while `anyUnacked`, a 500 ms `lv_timer` toggles the bg opacity between 100 % and 35 %.
- **When all are acknowledged:** steady, cycling the sorted list every 3 s (index shown as `n/N`).
- **Clickable:** `LV_EVENT_CLICKED` → `AlertManager::ackAll()` + `HapticService::click()`. `GESTURE_BUBBLE` is removed
  so a tap never becomes a page swipe (FR-007).
- **Rounded corners:** the band is a rectangle across the top 51 px. The text is centred, and its width is limited
  to the safe width at y = 25 px, which is ~340 px for r = 64. Longer text is shortened with LVGL's
  `LV_LABEL_LONG_DOT`.
- **Redraw:** only on snapshot change or the flash/cycle timers. Small area, no PFD impact (measured, SC-006).

## R6 · Clicks on controls
- **Decision:** a helper `HapticService::attachClick(lv_obj_t*)` adds an `LV_EVENT_CLICKED` / `VALUE_CHANGED` callback
  that calls `click()`. It is used on:
  - Settings buttons, switch, dropdown, zone/station OK;
  - WX REFRESH;
  - the annunciator.

  The helper lives in `src/ui/` (it wraps an LVGL call), and the service stays LVGL-free.
- **Page change:** `PageManager::onLoaded` calls `HapticService::click()` (FR-010: "within 50 ms of the page
  settling"; the side-button home path goes through the same callback).

## R7 · Settings (schema v5)
- `hapticlvl`: U8 0..2 (off/low/high), default 1, editable.
- `quiet`: Bool, default 0, editable.
- The migration from v4 is additive.
- The Settings page gets an "ALERTS" section: "Vibration" dropdown (Off/Low/High), "Quiet mode" switch, "Test
  alert" button (raises `Test1` CAUTION for 10 s so the wearer can feel the patterns).

## R8 · Performance and power
- A WARNING repeat is one I2C sequence every 2 s from the svc tick: negligible CPU.
- The ERM motor draws ~60–90 mA while running. The duty cycle while STALL is unacknowledged is about 75 % (1.5 s of
  every 2 s). It is bounded by acknowledgement, and it is the intended trade-off (constitution III: duty cycle
  stated). After acknowledgement it is 0.
- The flashing band redraws a 410×51 area at 2 Hz: negligible next to the PFD's 30 fps. Measured in Q6.
