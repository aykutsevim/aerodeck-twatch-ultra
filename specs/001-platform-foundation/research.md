# Research: Platform Foundation (S0)

All findings were verified against the installed toolchain (PlatformIO Core 6.2.0, Arduino-ESP32
2.0.17) and the LilyGoLib 0.2.0 sources in `.pio/libdeps/twatch_ultra/LilyGoLib` on 2026-11-11.

---

## R1. Making the LilyGoLib IDF-4.4 patch reproducible (FR-022, Principle VII)

**Decision**: A PlatformIO **`post:`** extra script, `tools/patch_lilygolib.py`. It patches every
`$PROJECT_LIBDEPS_DIR/$PIOENV/LilyGoLib/src/LilyGoDispInterface.cpp` idempotently: it replaces the
guard `#if (ESP_ARDUINO_VERSION <= ESP_ARDUINO_VERSION_VAL(4,0,0))` block with an
`ESP_IDF_VERSION >= 5.0.0` guard and always sets `vendor_config = nullptr`. The script detects
the already-patched marker and does nothing if found. If the original text is not found
(library changed), it fails the build with a clear message.

**Rationale**: Verified in PlatformIO source:
`builder/main.py:167` runs `pre:` scripts, then `:173` runs the platform `$BUILD_SCRIPT`. Inside it,
`BuildProgram → ProcessProjectDeps → ConfigureProjectLibBuilder` calls
`project.install_dependencies()` (`builder/tools/piolib.py:1204`). Then `:180` runs `post:`
scripts. SCons only compiles after every SConscript has been read. So on a clean checkout:
- a `pre:` script runs **before** LilyGoLib exists, so it cannot patch;
- a `post:` script runs **after** installation and **before** compilation, which is the correct window.

**Alternatives considered**:
- `pre:` script that shells out to `pio pkg install` first: works, but it's slower and recursive.
- Vendoring LilyGoLib into `lib/`: deterministic, but it copies a large library and loses the
  version pin's upgrade path.
- Forked Git URL: needs maintaining a fork. Kept as a fallback if the post-script fails verification.
- Migrating to Arduino-ESP32 3.x: removes the need, but is a large change outside S0 (SRS OI-5).

This deviates from AD-SAS-001 §9's suggested `pre:` → recorded as **ADR-008**.

---

## R2. Peripheral presence & simulated failure (FR-014, FR-015, FR-021)

**Decision**: Use the return value of `LilyGoUltra::begin(uint32_t disable_hw_init)`. It returns a
`devices_probe` bitmask (`HW_RADIO_ONLINE`, `HW_NFC_ONLINE`, `HW_GPS_ONLINE`, `HW_SD_ONLINE`,
`HW_DRV_ONLINE`, `HW_BHI260AP_ONLINE`, `HW_RTC_ONLINE`, `HW_TOUCH_ONLINE`, …). Simulated failures
pass the matching `NO_HW_*` flags (`NO_HW_LORA`, `NO_HW_NFC`, `NO_HW_GPS`, `NO_HW_SD`, `NO_HW_MIC`,
`NO_HW_DRV`, `NO_HW_SENSOR`, `NO_HW_RTC`) and mark the peripheral FAILED (simulated). The
simulation mask is a persisted diagnostic setting, editable via serial command, applied on next boot.

**Rationale**: No driver changes needed. It exercises the real "device not online" path.

**Alternatives considered**: Faking failures inside BoardUltra after a successful init. Rejected:
it doesn't exercise the vendor init skip path, and it leaves the hardware powered.

---

## R3. Core-component failure without a boot loop (FR-016, US3 scenario 4)

**Finding**: `LilyGoUltra::begin()` calls `assert(0)` if the AXP2101 PMU is not found
(`LilyGoWatchUltra.cpp` ~line 209). That would abort, reset and loop.

**Decision**: Before calling `begin()`, `BoardUltra` probes I2C address `0x34` (PMU) with a plain
`Wire` transaction. If it is absent: log FATAL over serial every 5 s and stay in a safe idle loop
(no reset). Also keep a boot counter in `RTC_NOINIT` memory. If 3 consecutive boots end in a
panic or watchdog reset within 10 s of boot, the next boot enters **safe mode**: only display,
touch and PMU are started (all other `NO_HW_*` flags set), and a caution "SAFE MODE" is shown.

**Display failure (analysis U1)**: `beginLvglHelper()` and the display driver also `assert()` (e.g.
`assert(buf)` on draw-buffer allocation). Safe mode alone would still start the display and loop.
So the boot record also stores a **stage breadcrumb** (`lastStage`), written before each boot
stage. If 3 fast fails happen with `lastStage == DisplayInit`, the next boot goes **Headless**: it
skips display and LVGL init entirely, runs only the serial console, and logs FATAL every 5 s. This
covers spec US3 scenario 4 for both core components (PMU and display).

**Rationale**: This satisfies "no restart loop" even for faults outside our probe.

**Alternatives considered**: Patching LilyGoLib to remove the assert. Rejected: it widens the
vendor patch surface.

---

## R4. Physical button mapping (FR-003)

**Finding**: On the Ultra, the user button is the AXP2101 power key. LilyGoLib turns its IRQ
into `POWER_EVENT` with `PMU_EVENT_KEY_CLICKED` (short press) or `PMU_EVENT_KEY_LONG_PRESSED`.
These are delivered through the `LilyGoEventManage` callback after `instance.loop()` handles
`HW_IRQ_POWER`. A hardware long press (several seconds) powers off the device at PMU level.

**Decision**: Short press (`PMU_EVENT_KEY_CLICKED`) = the FR-003 navigation button. Long press is
left to the PMU/hardware (power off). `instance.loop()` runs in the UI loop every iteration.
`BoardS3` maps the S3's equivalent PMU key the same way.

**Alternatives considered**: The BOOT (GPIO0) button. Rejected: it isn't intended as a user
control and is needed for download mode.

---

## R5. Settings persistence (FR-007, FR-009–FR-013)

**Decision**: ESP32 `Preferences` (NVS), namespace `aerodeck`. One key per setting plus
`schema` (uint16). Per-key typed writes. NVS commits are atomic per key, which meets "old or new,
never mixed" per setting. Values are validated against range on load, and out-of-range values
fall back to the default. Schema mismatch: if older, run migrations; if newer or unknown, reset
to defaults and raise a caution. Secret settings carry a flag that the logger and UI honour (show
`••••`). "Restore defaults" is `Preferences::clear()` plus a schema rewrite.

**Rationale**: NVS is wear-levelled and power-loss safe by design, and it's built into Arduino-ESP32.

**Alternatives considered**: JSON file on FAT with write-temp-then-rename. More code and more
wear, and FAT rename atomicity is weaker.

---

## R6. Filesystem & partition table (SAS §7.1)

**Finding**: LilyGoLib mounts **FAT (`FFat`) on partition `ffat`** in `begin()` (`setupMSC`) and
exposes it as **USB mass storage**. The current `board_build.partitions` points into `.pio/` (a
constitution violation).

**Decision**: Copy the factory layout into a project-owned `partitions/aerodeck_16MB.csv`
(nvs 20 KB, otadata, app0/app1 4 MB each, ffat ≈7.9 MB, coredump 64 KB). **Use FAT, not
LittleFS**, for assets in later slices. That way WAV files can be drag-and-dropped over USB.

**Rationale**: It reuses vendor behaviour instead of fighting it, and the user-friendly asset
upload is a bonus.

**Alternatives considered**: LittleFS as SAS §7.1 states. Rejected: it would need
`NO_INIT_FATFS` plus a second filesystem, for no benefit. Recorded as **ADR-009** (SAS §7.1
amended in this branch).

---

## R7. Display buffers & memory budget (FR-018, NFR-PERF-005)

**Finding**: `beginLvglHelper()` with DMA enabled allocates **two internal DMA buffers** of
`502×410/6×2 B ≈ 68.6 KB` each (≈137 KB internal RAM). Without DMA it uses two full-frame PSRAM
buffers (~412 KB each). LVGL heap is `LV_STDLIB_CUSTOM` (LilyGoLib's `lv_conf.h`).
`LV_DEF_REFR_PERIOD = 33 ms` (≈30 FPS cap). `LV_USE_OS = LV_OS_NONE`.

**Decision**: Keep the vendor helper and DMA (partial refresh) for S0. Measure free internal heap
with the overlay. If it drops below 40 KB, fall back to PSRAM buffers (one setting flag) and record
the measurement. The refresh period stays at 33 ms in S0 (30 FPS target). Raising the cap for a
60 FPS stretch is an S1 decision.

**Rationale**: Measure first (Principle IV). The ~137 KB used still leaves room on the ESP32-S3.

---

## R8. Task model & LVGL ownership (FR-018, Principle II)

**Decision**: The UI runs in the Arduino `loop()` (core 1): `instance.loop()` → drain EventBus
UI queue → `lv_timer_handler()`, then yield. A `svc` FreeRTOS task (core 0, prio 2, 50 ms tick)
hosts PowerService polling (1 Hz PMU read), diagnostics and the settings writer. All I2C access
from `svc` takes the board I2C mutex. LVGL is only called from `loop()`
(`LV_USE_OS = LV_OS_NONE` makes this mandatory anyway).

**Finding/risk**: Touch is read by LVGL's indev callback on core 1 via LilyGoLib, which doesn't
take our mutex. **Mitigation**: `BoardUltra` wraps the LilyGoLib touch read callback with the
same mutex (it registers its own `lv_indev` read callback that locks, then delegates).

---

## R9. Diagnostics overlay & logging (FR-019, FR-020)

**Decision**: A custom small overlay label updated by an `lv_timer` at 1 Hz, showing FPS from a
flush counter (via `lv_display` `LV_EVENT_RENDER_READY` events), free internal heap
(`heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`), its minimum-ever, and free PSRAM. The `Log`
module wraps `Serial.printf` with `[ms][LEVEL][tag]` and a runtime level, and redacts secrets by
API design: secret values are never passed to log calls; `SettingsStore` logs keys only.

**Alternatives considered**: LVGL's `LV_USE_SYSMON`/perf monitor. It needs `lv_conf` changes in
the vendor-supplied config, and it can't show internal-vs-PSRAM split. May be added later.

---

## R10. Native unit tests on Windows (FR-024, Principle V)

**Finding**: **No host C/C++ compiler is installed** (`gcc`/`g++` not found). PlatformIO
`platform = native` needs a system GCC (on Windows: MinGW-w64 on PATH).

**Decision**: Add `[env:native]` (`platform = native`, `test_framework = unity`,
`build_src_filter` limited to `src/core/`). Document the prerequisite (MSYS2 `mingw-w64-ucrt-x86_64-gcc`
or WinLibs, with `gcc` on PATH) in quickstart. Core logic avoids anything platform-specific.

**Note**: The existing upstream `env_emulator` (SDL2 LVGL emulator) entries are unrelated, and
they are removed from `platformio.ini` to keep the project focused (along with the `tlora_pager`
env and the upstream example `src_dir` comments).

---

## R11. Page navigation implementation (FR-002, FR-005, edge cases)

**Decision**: `PageManager` owns one LVGL screen per page, created lazily (`onCreate`) and kept.
Navigation uses `lv_screen_load_anim()` with `MOVE_LEFT/RIGHT`, 200 ms. Swipes are detected via
`LV_EVENT_GESTURE` on each page's screen, plus `lv_indev_get_gesture_dir`. Input is ignored while
a transition is running (a "busy" flag cleared on `LV_EVENT_SCREEN_LOADED`). A button press during
a transition is queued and applied afterwards (ends on home). Page indicator = dots in an overlay on
`lv_layer_top()`, hidden by a one-shot timer after 1 s.

**Rationale**: These are built-in LVGL 9 mechanisms. Keeping screens alive avoids re-creation cost
(memory is checked by the overlay).
