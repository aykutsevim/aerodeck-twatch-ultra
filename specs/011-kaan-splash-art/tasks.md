# Tasks: KAAN Splash Screen and Watch-Face Emblem

**Input**: Design documents from `specs/011-kaan-splash-art/`

**Prerequisites**:
- [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md);
- contracts: [art_assets](contracts/art_assets.md), [board_splash](contracts/board_splash.md),
  [serial_commands](contracts/serial_commands.md);
- [quickstart.md](quickstart.md).

**Tests**: required by constitution V (pure core logic lands with Unity tests, written first) and by the on-device
acceptance checks (quickstart Q0–Q5).

**Conventions**:
- Run PlatformIO as `& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio …`. Never run two pio
  builds at once.
- Repo files are CRLF in the working tree.
- Device on COM3: use the scratchpad serial helpers, DTR/RTS low.
- **Never** `ad set reset` on the device (it wipes the Wi-Fi settings).

## Phase 1: Setup

- [X] T001 Baseline on branch `011-kaan-splash-art` before any code:
  - `pio test -e native` (expect 305/305);
  - `pio run -e twatch_ultra` and `-e twatchs3` (RAM/flash);
  - on the device: `ad reboot` ×3, then `ad boot`, recording `boot_to_home_ms` (S9 value 3 776 ms);
  - one eject + button wake, recording `boot_to_home_ms wake=button`.

  Write all of this to a new `specs/011-kaan-splash-art/validation.md` (section T001).
- [X] T002 Add `+<assets/>` to `build_src_filter` of `[env:native]` in `platformio.ini`, so the generated asset unit
  compiles in the host tests. The Ultra/S3 envs already build all of `src/`.

## Phase 2: Foundational (blocking: assets + pure core used by both stories)

- [X] T003 Write the generator `tools/gen_kaan_art.py` per [contracts/art_assets.md](contracts/art_assets.md):
  - Python 3 calling PowerShell / .NET `System.Drawing` (no Pillow) to load `art/kaan-splash-screen.jpg`, compute
    the luminance bounding box (> 48), add a 4 % margin, crop, then resample with
    `InterpolationMode.HighQualityBicubic` to (a) the splash size and (b) the emblem size, and dump the grayscale bytes;
  - map luminance → A8 (black 16 / white 235);
  - splash target: the largest size with the crop's aspect such that every α > 0 pixel lies inside the 410 × 502
    rounded rectangle (r = 64) with ≥ 8 px margin (the tool checks this per pixel);
  - emblem target: height = `h − m − rowY` from the portrait `PfdLayout` formula for 410 × 502 (reimplemented in the
    tool and printed); width by aspect, capped at `contentW / 4`;
  - `--emblem-aircraft-only` option;
  - write `src/assets/KaanArt.h` (`#include "core/AlphaArt.h"` + `extern const AlphaArt kKaanSplash, kKaanEmblem;`)
    and
    `src/assets/KaanArt.cpp` (`const uint8_t` arrays, 32 bytes per line, a header comment with the source size, crop
    box and target sizes), in CRLF;
  - deterministic output;
  - before writing it, create the hand-written `src/core/AlphaArt.h` (`struct AlphaArt { uint16_t w; uint16_t h;
    const uint8_t* a8; };`, `<cstdint>` only), shared by the assets, `IBoard`, `SplashCompose` and `PfdPage`;
  - state in the tool's docstring that the sizes target the Ultra (410 × 502, r = 64) and must be re-run for another
    screen.
- [X] T004 Run `python tools/gen_kaan_art.py`, commit-ready output in `src/assets/KaanArt.{h,cpp}`. Run it a second
  time and confirm `git diff --stat` shows no change. Record the sizes and crop box in `validation.md` (Q0).
- [X] T005 [P] Tests first: create `test/test_splash_compose/test_main.cpp` (Unity) for `core::SplashCompose`
  ([data-model.md](data-model.md)):
  - α 0 → black, α 255 → fg, blending monotonic in α;
  - a row above/below the art is all black;
  - centring for even and odd size differences;
  - byte swap on/off;
  - `fitsRounded()` true for a centred small mask and false for one touching a corner;
  - `shouldShow(mode, wake, hasArt)` true only for Normal + PowerOn + art. False, each with its skip reason, for:
    Safe and Headless → `Safe`; Button, Imu, Timer and **Other** → `Wake`; no art → `NoArt`;
  - asset sanity: `kKaanSplash` / `kKaanEmblem` non-empty, aspect within 2 % of each other, and
    `fitsRounded(kKaanSplash, 410, 502, 64)`.
- [X] T006 [P] Tests first: extend `test/test_pfd_layout/test_main.cpp` with emblem tests. Ultra
  `compute(410, 502, 64, aspect)`:
  - non-empty;
  - centred ±1 px;
  - top == the ZULU row top;
  - bottom ≤ h − m;
  - inside the rounded screen;
  - no overlap with `eicas`, `adi`, `hour`, `minute`;
  - no overlap with the ZULU/LOCAL text zones (each box centre ± 55 px);
  - width ≤ contentW / 4.

  Plus: `kKaanEmblem` fits the Ultra emblem box ±1 px; S3 `compute(240, 240, 0, aspect)` gives an empty emblem; the
  existing tests are unchanged.
- [X] T007 Implement `src/core/SplashCompose.{h,cpp}`:
  - `enum class SplashSkip { None, Wake, Safe, NoArt, NoMem }` plus `skipName()` → `"wake" | "safe" | "none" |
    "nomem"`;
  - `row(const uint8_t* a8, uint16_t aw, uint16_t ah, uint16_t W, uint16_t H, uint16_t y, uint16_t fg565,
    bool swap, uint16_t* out)`;
  - `fitsRounded(a8, aw, ah, W, H, r, margin)`;
  - `shouldShow(BootMode, WakeCause, bool hasArt, SplashSkip& why)`.

  No Arduino; uses only `core/` types (`BootMode` from `core/BootGuard.h`, `WakeCause` from its core header). T005
  must pass.
- [X] T008 Implement `PfdLayout::emblem` in `src/core/PfdLayout.{h,cpp}`:
  - new `Box emblem`;
  - `compute(w, h, cornerR = 0, emblemAspect = 0.0f)`, with aspect 0 meaning no emblem;
  - portrait rules per data-model;
  - landscape: empty.

  T006 must pass; the full `pio test -e native` suite stays green.

**Checkpoint**: the assets are generated; the native suite is green with the new tests.

## Phase 3: User Story 1 — KAAN splash at start-up (P1) 🎯 MVP

**Goal**: full-screen KAAN art from the moment the panel is ready until the first PFD frame, on full starts only, with
no extra boot time.

**Independent test**: `ad reboot` → the splash, then the PFD; `ad boot` shows `splash=1` and `boot_to_home_ms` ≤ 4000.
After an eject + wake: `splash=0 skip=wake`.

- [X] T009 [US1] Add `SplashSpec`, `SplashResult`, virtual `setSplash()` and `splashResult()` (no-op defaults) to
  `src/board/IBoard.h` per [contracts/board_splash.md](contracts/board_splash.md). For the `AlphaArt` type include
  `core/AlphaArt.h` (not the generated asset header). `SplashSkip` comes from `core/SplashCompose.h`.
- [X] T010 [US1] *(Superseded during implementation: the split was built, measured at ≈ 275 ms visible, and
  replaced by LilyGoLib's boot image + AERODECK-PATCH-3, research R2 revision. LilyGoLib's init order is
  unchanged.)* Original task: BoardUltra init split in `src/board/BoardUltra.cpp` (`begin()`):
  - add `NO_HW_TOUCH | NO_HW_RTC | NO_HW_MIC` to the `instance.begin()` flags, remembering which of them were
    **already** set by the SimFail/safe masks;
  - after it returns: `showSplash()` (T011);
  - then `instance.initTouch()`; `instance.initRTC()` unless it was masked; `instance.initMicrophone()` +
    `instance.initAmplifier()` unless mic was masked;
  - OR `HW_TOUCH_ONLINE` / `HW_RTC_ONLINE` / the mic and codec bits into `probe_` from the return values;
  - keep `audioReady_` meaning "amplifier initialised".

  Every start uses this path, with or without the splash.
- [X] T011 [US1] *(As implemented: `prepareSplash()` composes the full frame in PSRAM on core 0 and registers it
  with `setBootImage()`; the hook `aerodeck_boot_image_shown()` sets the brightness after 2 ms;
  contracts/board_splash.md.)* Original task: `BoardUltra::showSplash()` in `src/board/BoardUltra.cpp`:
  - decide with `core::SplashCompose::shouldShow(mode, wakeCause_, spec.art != nullptr, why)`;
  - when shown: `heap_caps_malloc` a 32-row RGB565 strip buffer in PSRAM (on failure: skip `NoMem` + LOGW);
  - for each strip, `SplashCompose::row` with swap on, then `instance.pushColors(0, y, w, rows, buf)`;
  - free the buffer;
  - `instance.setBrightness(pct × DEVICE_MAX_BRIGHTNESS_LEVEL / 100)` (under `I2cLock` like `setBrightnessPct`);
  - fill `SplashResult{shown, atMs = millis(), costMs, skip}`;
  - log `[boot] splash shown at %lu ms (%lu ms)` / `[boot] splash skipped (%s)`.

  Store the safe-mode flag from `begin()`'s argument for the rule.
- [X] T012 [US1] In `src/app/main.cpp` before `board().begin()`, build a `SplashSpec`:
  - `art = &kKaanSplash`;
  - `fg565`: NVG on → `core::NvgColor::toGreen565(0xFFFF)`, else `0xFFFF`;
  - `brightnessPct`: the same rule as `applyBrightness()` (NVG 10, else `(Bright + 1) × 20`), factored into one helper
    used by both;
  - call `board().setSplash(spec)`.
- [X] T013 [US1] Extend `ad boot` in `src/app/main.cpp` per
  [contracts/serial_commands.md](contracts/serial_commands.md):
  - `splash=1 splash_at_ms=<t> splash_cost_ms=<c>`;
  - or `splash=0 skip=<wake|safe|none|nomem>`.
- [X] T014 [US1] Check that the first PFD frame fully covers the splash. If any splash pixels remain (e.g. outside the
  PFD screen's objects), call `lv_obj_invalidate(lv_screen_active())` once before the first frame in
  `PageManager::begin()`/the first-home path in `src/app/main.cpp` and re-test.
- [X] T015 [US1] Build and flash the Ultra; on-device **Q1**:
  - `ad reboot` ×5 (each is a full start, `wake=poweron`). A cold power-on (the PMU power key held to switch off, then
    on) is optional and only if the wearer wants to try it. Eject is deep sleep, not power-off, and is covered by T016;
  - record per start `boot_to_home_ms`, `splash_at_ms`, `splash_cost_ms` and the visible window;
  - all ≤ 4000 ms;
  - wearer checks (art complete, undistorted, not cut by the corners; NVG reboot shows green and dim; no white flash).

  Record verbatim in `validation.md`. If the visible window is very short (< 0.3 s), report it to the wearer with the
  numbers (no hold may be added: FR-003).
- [X] T016 [US1] On-device **Q2**:
  - eject + button wake ×3 → `splash=0 skip=wake`; the **mean `boot_to_home_ms` is within 50 ms of the T001 wake
    value** (SC-002);
  - `ad probe s8 alarm 20` timer wake → `skip=wake`.
- [X] T017 [US1] On-device **Q3**:
  - `ad simfail rtc` + `ad reboot` → the splash shows, then the PFD with the RTC CAUTION;
  - `ad simfail mic` likewise (audio CAUTION/state as in T001);
  - `ad simfail none` + `ad reboot`; `ad periph` all as in T001.

  Record in `validation.md`.

**Checkpoint**: US1 is shippable alone (the splash works; the PFD is unchanged).

## Phase 4: User Story 2 — KAAN emblem on the PFD (P2)

**Goal**: the small KAAN emblem at the bottom centre between ZULU and LOCAL, night-mode aware, with no fps cost.

**Independent test**: the PFD shows the emblem without overlap; `ad stat` fps ≥ 30 with `ad att sweep on`; NVG
recolours it.

- [X] T018 [US2] In `src/pages/PfdPage.cpp`:
  - pass `float(kKaanEmblem.w) / kKaanEmblem.h` to `PfdLayout::compute`;
  - if `L.emblem.w > 0`, create an `lv_image` from an `lv_image_dsc_t` (color format `LV_COLOR_FORMAT_A8`, stride =
    w, data = `kKaanEmblem.a8`; a static descriptor in the page);
  - position and size it at `L.emblem`, `image_recolor = Theme::fg()`, `image_recolor_opa = LV_OPA_COVER`,
    `opa = LV_OPA_80`;
  - not clickable; no gesture-bubble change;
  - create it before the eject handle so the handle stays on top.
- [X] T019 [US2] Build and flash; on-device **Q4**:
  - the emblem is centred between ZULU and LOCAL with no overlap (wearer + description);
  - `ad att sweep on` + `ad stat` ×3 → fps ≥ 30, heap ≥ 40 KB;
  - `ad nvg on` → the emblem is green (wearer), then `ad nvg off`;
  - wearer questions:
    - is it recognisable, and is the lettering acceptable? If not → regenerate with `--emblem-aircraft-only`, rebuild
      and re-check;
    - are the colour and brightness right (white at 80 % opacity)? If not → adjust the recolour/opacity in
      `PfdPage.cpp` (e.g. `Theme::dim()` or 60 %) and re-check.

  Record verbatim in `validation.md`.

**Checkpoint**: both stories done.

## Phase 4b: User Story 3 — KAAN page (P3, added by the user during implementation)

- [X] T024 [US3] `src/pages/KaanPage.{h,cpp}`:
  - `PageId::Kaan` (`src/ui/Page.h`);
  - the `kKaanSplash` A8 image centred, recoloured `Theme::fg()`, not clickable (swipes go to the screen);
  - registered last in `src/app/main.cpp`, so `prev()` from the PFD wraps to it;
  - checks: on the device PFD→KAAN on a swipe right (`nav prev`), KAAN→PFD, SETTINGS↔KAAN; the boot still lands on
    the PFD; heap unchanged; wearer check.

## Phase 5: Polish & Cross-Cutting

- [X] T020 [P] Docs:
  - README: the splash on full starts, the PFD emblem, `art/` + `tools/gen_kaan_art.py` (regeneration), the new
    `ad boot` fields;
  - `docs/software_architecture_specification.md`: the boot sequence (splash right after panel init, init split) and
    §8 PFD emblem;
  - `docs/software_requirements_specification.md`: an FR-PLT-001 note (the splash is shown inside the 4 s limit;
    measured values).
- [X] T021 On-device **Q5** regression:
  - swipe through all pages;
  - the PFD eject handle still works (pull to eject + wake);
  - TCAS page OK;
  - `ad boot` wake timings vs T001;
  - checks for the T010 init split (constitution I, nothing shipped may break):
    - **audio** (S5): `ad audio play traffic` plays (amp ON, `play traffic`), and the wearer hears it;
    - **touch**: after a button wake from eject, a tap/swipe works on the first try;
    - **RTC** (S2/S8): the boot log shows `[time] RTC … valid`; `ad time` is correct; the S8 RTC-alarm wake from
      T016 worked;
    - `ad periph` matches T001 line by line.
- [X] T022 Final checks:
  - `pio test -e native` all green;
  - both builds; the flash/RAM delta vs T001 (expect ≈ +150 KB flash);
  - generator idempotent (no diff);
  - complete `validation.md` with SC-001…SC-006 and evidence;
  - mark all tasks [X].
- [X] T023 At merge: update `docs/product_roadmap.md`. This is not a numbered roadmap slice: add a dated note/row
  "KAAN splash + PFD emblem (feature 011)" with the PR number, in the status area next to S9.

---

## Dependencies & Execution Order

- **T001 → T002 → T003 → T004** (the assets are needed by T005's sanity test and by T006's fit test).
- **T005 ∥ T006** (different test folders) → **T007** (needs T005) and **T008** (needs T006); T007 ∥ T008.
- **US1 (T009–T017)** needs T007; **US2 (T018–T019)** needs T008 and T004. US2 does not depend on US1.
  US2 touches only `PfdPage.cpp`, so US1 and US2 can proceed in parallel after Phase 2.
- **Polish (T020–T023)** after both stories; T023 at merge.

## Parallel Opportunities

- T005 ∥ T006 (tests); T007 ∥ T008 (separate core modules).
- After Phase 2: T009–T014 (board/main) ∥ T018 (PfdPage). The on-device checks run one at a time (one serial port).
- T020 (docs) ∥ the on-device checks.

## Implementation Strategy

1. **MVP = US1**: assets + `SplashCompose` + the init split + the splash push. It is shippable alone, with the boot
   limit verified.
2. **US2**: the emblem is a small, isolated `PfdPage` change on top.
3. Validate with the wearer, then polish and merge.
