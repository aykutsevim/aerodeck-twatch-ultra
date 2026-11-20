# Research: KAAN Splash Screen and Watch-Face Emblem (feature 011)

Measured on the T-Watch Ultra, 2026-11-17, `main` after S9 (`ad reboot` boot log):

| Point | Time |
| :-- | :-- |
| `setup()` first log (`AeroDeck starting`) | 1 336 ms |
| `board().begin()` returns (first service log) | ≈ 3 640 ms |
| First PFD frame (`boot->home`) | **3 776 ms** (limit FR-PLT-001: 4 000 ms; margin ≈ 220 ms) |

## R1 — When can a splash appear?

- **Decision**: push the splash **directly to the panel right after LilyGoLib's panel init**, before touch, RTC,
  microphone and amplifier init, LVGL and the services. It stays until LVGL's first PFD frame overwrites it.
- **Rationale**: LVGL is ready only at ≈ 3.64 s, and the PFD replaces anything at 3.78 s, so an LVGL splash would be
  invisible (≈ 0.14 s) or would need a hold that breaks the 4 s limit (FR-003). LilyGoLib initialises the panel in the
  middle of `instance.begin()` (`LilyGoDispQSPI::init`), after PMU, expander and haptic-driver init. After it,
  `begin()` still runs touch, RTC and (not deferred) microphone and amplifier init. IMU, NFC, GNSS, LoRa and SD are
  already deferred in AeroDeck. Then LVGL, the services and the first PFD frame follow. The visible splash window is
  therefore "panel ready → first PFD frame"; it is **measured in the implementation** (`ad boot` reports it, R7). No
  extra wait is added.
- **Alternatives rejected**:
  - LilyGoLib's own boot image (`setBootImage`): it pushes at the same point but then calls
    `incrementalBrightness(250, 20)`, a **synchronous 5 s fade** (250 steps × 20 ms). Using it needs a library patch.
  - Splash through LVGL + hold ≥ 1 s: breaks FR-PLT-001 (boot ≈ 4.8 s).
  - Moving vendor init around inside LilyGoLib (patch): more patching for little gain.

## R2 — Getting control right after panel init without patching LilyGoLib

- **Decision**: call `instance.begin()` with `NO_HW_TOUCH | NO_HW_RTC | NO_HW_MIC` added to the flags. When it
  returns the panel is ready. BoardUltra then (a) pushes the splash and sets the brightness, if a splash is wanted,
  and (b) calls the skipped inits itself in LilyGoLib's order: `initTouch()`, `initRTC()`, `initMicrophone()`,
  `initAmplifier()` (all public). It ORs the matching `HW_*_ONLINE` bits into the probe result from their return
  values (`devices_probe` is private).
- **The same path is used on every start** (splash or not), so there is one init order to test. The order of
  peripheral init is unchanged; only the split point moves.
- **Rationale**: no vendor patch (constitution VII prefers none). LilyGoLib 0.2.0 is pinned, so the public init
  functions are stable.
- **Alternative rejected**: a `patch_lilygolib.py` patch replacing the fade with an immediate brightness. It works, but
  it touches vendor code, and the boot-image member is still pushed only from inside `begin()`.

### R2 revision (implementation, measured; supersedes R1/R2 above)

- The init split worked, but the splash showed only **≈ 275 ms**: the panel was ready at ≈ 2.8 s, yet
  `instance.begin()` then spent ≈ 0.6 s in `esp_enable_slow_crystal()` (32 kHz crystal calibration) before
  returning. A debug-log build showed this; the panel reset itself takes 700 ms of fixed delays.
- **Decision (final):** use LilyGoLib's boot image (pushed between the panel init and the calibration) with
  **AERODECK-PATCH-3**. The patch replaces the synchronous 5 s fade with a weak hook
  `aerodeck_boot_image_shown()`, which BoardUltra implements (2 ms wait, the user's brightness, timestamp). The
  splash is composed on core 0 in parallel. Visible ≈ **1.07 s**, boot +57 ms (≈ 3.88 s ≤ 4 s). LilyGoLib's
  init order is unchanged, so the init split was **reverted** (no regression risk to touch, RTC or audio).
- Reason for the vendor patch: it gives 4× the visible time for a one-line, marker-guarded, automatic patch
  (constitution VII allows required patching done by the build).
- **SPI bus-lock race** (validation T014): a polling panel command right after a DMA push panics a later flush
  (`spi_bus_lock_bg_exit`). Guarded by a 2 ms wait in the hook and in `waitFlushIdle()`.

## R3 — When the splash is shown

- **Decision**: only when **all** of these are true: `wakeCause == PowerOn` (power-on, reset, `ad reboot`, a firmware
  update: everything that is not a deep-sleep wake); boot mode **Normal** (not Safe, not Headless); the art is
  linked in. A wake from eject (`Button`, `Imu`, `Timer` = S8 RTC alarm) never shows it (FR-004).
- **Rationale**: keeps the S7/S8 wake-to-time path unchanged. Skipping it in safe mode means a boot that has already
  failed does not run extra code.

## R4 — Art preparation (build-time, committed output)

- **Decision**: an offline generator `tools/gen_kaan_art.py` (Windows, like `gen_audio.py`). It decodes and resamples
  `art/kaan-splash-screen.jpg` with **.NET System.Drawing** through PowerShell (built into Windows, bicubic, no
  third-party package). It writes **`src/assets/KaanArt.{h,cpp}`** with two **8-bit alpha masks (A8)**:
  - **Splash**: the source is cropped to the white content's bounding box plus a 4 % margin. The black margins are
    plain background, so the art itself is unchanged. It is scaled to fit **inside 410 × 502 minus a corner-safe
    margin** (proportions kept, ≈ 300 × 470, ≈ 140 KB of flash).
  - **Emblem**: the same crop scaled to the emblem height (R6, ≈ 48 × 74, ≈ 3.5 KB).
  - The generated files are committed. The firmware build needs neither the tool nor the JPEG (VII: a clean build
    from checkout). The source JPEG stays in `art/` (documented).
- **Why A8**: the art is white on black, so luminance = alpha. One mask serves any colour (white by day, the NVG
  recolour at night) and is a third of the size of RGB565. LVGL 9 draws A8 images with `image_recolor` natively.
- **Alternatives rejected**:
  - Pillow: not installed (a new dev dependency).
  - RGB565 assets: 2× the size, and fixed colours.
  - Runtime JPEG decoding (TJpgDec): CPU and RAM at boot, and a decoder dependency.

## R5 — Composing and pushing the splash

- **Decision**: a pure function `core::SplashCompose::row(...)` renders one screen row: an A8 mask centred in W × H,
  blended `fg × α` over black, RGB565 with byte swap (the panel's order, as the flush does). In NVG the colour is
  run through `core::NvgColor`'s mapping, so the splash looks exactly like the NVG-filtered UI.
- BoardUltra fills a **32-row strip buffer in PSRAM** (410 × 32 × 2 = 26 KB), pushes strip by strip with
  `pushColors`, frees the buffer, then sets the brightness once (the stored level, or 10 % in NVG; the same value
  `applyBrightness()` sets later, so there is no flicker).
- Cost: ≈ 206 k pixels composed plus ≈ 410 KB over 80 MHz QSPI: an estimated **≤ 40 ms**, to be measured (R7).
- **Rationale**: no 410 KB buffer (constitution IV: large buffers in PSRAM, kept small anyway), and testable off
  device (V).

## R6 — Emblem placement on the PFD

- Ultra portrait layout (`core::PfdLayout`, w = 410, s ≈ 1.71): ZULU and LOCAL boxes are each ≈ 194 px wide on the
  bottom row (`rowY` ≈ 417, `timeH` ≈ 58). Their text ("88:88" in DSEG7 28 px ≈ 100 px) is centred in each box, so
  the free gap between the two texts is ≈ 100 px wide, centred on the screen. Below the row ≈ 27 px of the screen
  remain (the bottom centre is not cut by the 64 px corner arcs).
- **Decision**: `PfdLayout` gains `emblem`. Centred at x = w/2; its top is `rowY`; its height reaches the bottom
  margin (`h − m − rowY`, ≈ 78 px); its width follows the art's aspect (≈ 0.64), capped at `contentW / 4`. In
  landscape / square layouts (T-Watch S3) there is no gap, so `emblem` is empty and not drawn.
- **Emblem colour**: `Theme::fg()` at 80 % opacity (white, slightly softer than the values). The NVG flush filter
  recolours it like everything else (FR-008). It is not clickable.
- **Frame time**: a static image outside the horizon's invalidated areas. It is redrawn only when the bottom row is
  invalidated (minute changes). FR-009 is checked with the fps overlay.
- **Legibility note**: at ≈ 74 px tall the "KAAN" lettering is ≈ 3 px thick. If the wearer finds it noisy, the
  generator can emit an aircraft-only emblem (a tool flag, wearer decision in validation).

## R7 — Measurement and validation hooks

- The boot log gains `[boot] splash shown at <ms> (<n> ms)` (or `splash skipped (<reason>)`).
- `ad boot` adds `splash=<0|1> splash_at_ms=<t>`, so the visible window is `boot_to_home_ms − splash_at_ms`.
- SC-001 / FR-003: `boot_to_home_ms` stays ≤ 4 000 ms over 5 restarts.

## R8 — T-Watch S3

- `IBoard::setSplash()` is a no-op on BoardS3 (240 × 240 panel; the S3 is a compile target, not worn). The emblem is
  empty in its landscape/square layout. The build stays green (VII).
