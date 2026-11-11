# ADR-008: Patch LilyGoLib with a PlatformIO `post:` extra script

- **Status:** Accepted (slice S0, 2026-11-11)
- **Supersedes:** AD-SAS-001 §9 suggestion of a `pre:` script

## Context
LilyGoLib 0.2.0 `src/LilyGoDispInterface.cpp` guards ESP-IDF-5-only `esp_lcd_panel_dev_config_t`
fields (`rgb_ele_order`, `data_endian`) with `#if (ESP_ARDUINO_VERSION <= ESP_ARDUINO_VERSION_VAL(4,0,0))`.
That is true on our Arduino-ESP32 2.0.17 / ESP-IDF 4.4 core, so the build fails. Constitution VII requires
a clean checkout to build with no manual steps.

PlatformIO Core 6.2 source shows the order of operations:
- `builder/main.py:167` runs `pre:` extra scripts.
- `builder/main.py:173` runs the platform build script. Inside it, `BuildProgram → ProcessProjectDeps →
  ConfigureProjectLibBuilder` calls `project.install_dependencies()` (`builder/tools/piolib.py:1204`).
- `builder/main.py:180` runs `post:` extra scripts.
- SCons compiles only after every SConscript has been read.

## Decision
`tools/patch_lilygolib.py` runs as a **`post:`** script. It rewrites the guard in
`$PROJECT_LIBDEPS_DIR/$PIOENV/LilyGoLib/src/LilyGoDispInterface.cpp` to test `ESP_IDF_VERSION >= 5.0.0`
(IDF 4.4 uses `color_space`), sets `vendor_config = nullptr`, and writes the marker `AERODECK-PATCH-1`.
- It is idempotent: if the marker is present it prints `already patched`.
- It also migrates the earlier manual bring-up patch to the canonical form.
- If neither the original nor the marked text is found, it fails the build with a message naming the file.

## Consequences
- `pio run -e twatch_ultra` / `-e twatchs3` work from a clean checkout (verified: a fresh `twatchs3` libdeps
  printed `patched (twatchs3)`; see `specs/001-platform-foundation/validation.md`).
- A LilyGoLib upgrade that changes this code makes the build fail loudly instead of silently mis-compiling.
- The patch surface is limited to this one guard. Other LilyGoLib behaviour (e.g. its `assert` on a missing PMU,
  fixed display-reset delays) is handled in our code, not by widening the patch.

## Alternatives
- `pre:` script: runs before `lib_deps` are installed on a clean checkout, so the file does not exist yet.
- Vendoring LilyGoLib under `lib/`: deterministic, but copies a large library and loses the version pin.
- A forked Git URL: needs maintaining a fork. Kept as the fallback.
- Arduino-ESP32 3.x / IDF 5: removes the need, but is a larger migration (SRS OI-5).
