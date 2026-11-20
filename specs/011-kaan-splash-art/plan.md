# Implementation Plan: KAAN Splash Screen and Watch-Face Emblem

**Branch**: `011-kaan-splash-art` | **Date**: 2026-11-17 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/011-kaan-splash-art/spec.md`

## Summary

The wearer's KAAN art (`art/kaan-splash-screen.jpg`) is shown in two places:

- **Full-screen splash at every full start.** BoardUltra pushes it straight to the panel the moment LilyGoLib has
  initialised it, before the rest of start-up. It stays until the first PFD frame replaces it, so it adds no wait and
  the 4 s boot limit is kept (research R1, R2).
- **Small emblem on the PFD**, at the bottom centre between ZULU and LOCAL, placed by `core::PfdLayout`.

The art is converted offline into two committed 8-bit alpha masks (`src/assets/KaanArt.*`). They are recoloured at
run time, so the NVG night colours work for both.

## Technical Context

**Language/Version**: C++17 (Arduino-ESP32 2.0.17 / ESP-IDF 4.4, PlatformIO); Python 3 + PowerShell 5.1 for the
offline asset tool

**Primary Dependencies**: LilyGoLib 0.2.0 (pinned; public `initTouch`/`initRTC`/`initMicrophone`/`initAmplifier`,
`pushColors`, `setBrightness`); LVGL 9 (A8 image + `image_recolor`). No new libraries.

**Storage**: the assets live in flash (rodata), ≈ 145 KB. No NVS changes, no new settings.

**Testing**: Unity native tests (`pio test -e native`): `SplashCompose`, `PfdLayout.emblem`, asset sanity. On-device
quickstart Q1–Q5.

**Target Platform**: T-Watch Ultra (ESP32-S3, 410 × 502 AMOLED, QSPI); T-Watch S3 builds (no-op).

**Project Type**: embedded firmware (single project).

**Performance Goals**:
- boot to a usable PFD ≤ 4 000 ms (today 3 776 ms);
- splash compose + push ≤ 40 ms;
- PFD median ≥ 30 fps with the emblem.

**Constraints**:
- no extra boot wait;
- no splash on deep-sleep wakes;
- no vendor patch;
- internal heap unchanged (the strip buffer is in PSRAM and only lives during boot);
- the emblem must not overlap any PFD element.

**Scale/Scope**:
- 1 generator;
- 1 generated asset unit;
- 1 new core module;
- changes to `PfdLayout`, `IBoard`/BoardUltra, `PfdPage`, `main.cpp` and the `ad boot` command.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

| Principle | Status | How |
| :-- | :-- | :-- |
| I. Every slice ships | ✅ | the splash can never block the boot: skipped on any problem (no PSRAM, safe mode, wake); the PFD stays reachable; SimFail paths unchanged (Q3) |
| II. Layering | ✅ | the panel push lives in BoardUltra (only the board touches LilyGoLib); pure composition in `core::SplashCompose`; the PFD draws the emblem through LVGL on the UI task; the assets are data only; `main` passes a `SplashSpec` to `IBoard` |
| III. Battery | ✅ | no new consumers; the splash brightness is the user's level (10 % in NVG) |
| IV. Frame time | ✅ | the emblem is a static A8 image outside the horizon's redraw area; fps checked in Q4; the strip buffer (26 KB) is in PSRAM, only during boot |
| V. Test off-device | ✅ | `SplashCompose`, the emblem layout and the show rule (as a pure predicate) have native tests, written first |
| VI. Compliant/private | ✅ | the art is the wearer's own image, used for this personal device (spec Assumptions); no transmissions, no data |
| VII. Reproducible builds | ✅ | the generated assets are committed; the build needs no tool, JPEG decoder or patch; S3 compiles (defaults); layout sizes come from `PfdLayout` scaling |
| VIII. Simplicity | ✅ | no setting and no animation; one init path for every start; the generator is a standalone dev tool |

Post-design re-check: no violations, so no Complexity Tracking entries.

## Project Structure

### Documentation (this feature)

```text
specs/011-kaan-splash-art/
├── spec.md
├── plan.md              # this file
├── research.md          # R1–R8 (boot timing, init split, show rule, assets, composition, emblem, S3)
├── data-model.md        # AlphaArt, SplashSpec/Result, SplashCompose, PfdLayout.emblem
├── quickstart.md        # Q0–Q5
├── contracts/
│   ├── art_assets.md        # generator + generated asset format
│   ├── board_splash.md      # IBoard::setSplash / splashResult, BoardUltra init split
│   └── serial_commands.md   # `ad boot` fields, boot log lines
└── checklists/requirements.md
```

### Source Code (new or changed)

```text
art/kaan-splash-screen.jpg            # source art (committed)
tools/gen_kaan_art.py                 # NEW: offline generator (PowerShell/.NET resample → A8 arrays)
src/core/AlphaArt.h                   # NEW: struct AlphaArt {w, h, a8} (shared type, hand-written)
src/assets/KaanArt.h / KaanArt.cpp    # NEW (generated): kKaanSplash, kKaanEmblem (AlphaArt); sizes target the Ultra
src/core/SplashCompose.h / .cpp       # NEW: row composition (A8 → RGB565, swap), fitsRounded(), shouldShow(), SplashSkip (incl. NoMem)
src/core/PfdLayout.h / .cpp           # + Box emblem (portrait only), compute(..., emblemAspect)
src/board/IBoard.h                    # + SplashSpec / SplashResult / setSplash() / splashResult()
src/board/BoardUltra.cpp              # splash composed on core 0 → LilyGoLib boot image; hook; waitFlushIdle guard
tools/patch_lilygolib.py              # + AERODECK-PATCH-3 (boot-image fade → aerodeck_boot_image_shown hook)
src/pages/PfdPage.h / .cpp            # + emblem lv_image (A8, recolor fg, 80 % opa, not clickable)
src/app/main.cpp                      # SplashSpec from settings (NVG, brightness) before board().begin(); `ad boot` fields
test/test_splash_compose/             # NEW
test/test_pfd_layout/                 # + emblem tests
README.md, docs/software_architecture_specification.md   # splash + emblem notes; `ad boot` fields
```

**Structure Decision**: single embedded project. `src/assets/` is a new data-only folder; the generated code is kept
apart from hand-written core code.

## Implementation Notes (for /speckit-tasks)

1. **Generator first**, then commit the assets. It is deterministic, so a second run gives no diff. The tool computes
   the emblem height with the same formula as `PfdLayout` for 410 × 502 and prints it. A unit test asserts that
   `kKaanEmblem` fits `PfdLayout(410, 502, 64).emblem` (±1 px).
2. **Tests before code**: `SplashCompose` (alpha blend, centring, black rows, swap, `fitsRounded`, `shouldShow(mode,
   wake, art)`), and the `PfdLayout` emblem invariants (data-model).
3. **BoardUltra init split**: keep LilyGoLib's order (touch → RTC → mic → amp). Respect the existing SimFail/safe
   masks: a peripheral whose `NO_HW_*` flag was already set must stay skipped. Re-check `audioReady_` and the touch
   wrapper (`wrapTouchRead()` runs after LVGL, as before).
4. **Splash brightness**: `main` computes it with the same rule as `applyBrightness()`, so there is no flicker.
   `ScreenService::setApplied` is unchanged.
5. **First PFD frame** covers the full screen. Check that no strip of the splash survives (e.g. under the annunciator
   area). If one does, invalidate the whole screen on the first `onEnter`.
6. **`ad boot`** fields per the contract; the boot log lines.
7. **Docs**:
   - README: the splash and the emblem, `art/` and the generator;
   - SAS §8 (PFD emblem) and the boot sequence note (splash after panel init);
   - SRS FR-PLT-001 note (splash inside the 4 s).
8. **Validation file** `validation.md`: Q0–Q5 results, visible window, wearer quotes.

## Complexity Tracking

None.
