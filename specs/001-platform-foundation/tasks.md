---

description: "Task list for Platform Foundation (Slice S0)"
---

# Tasks: Platform Foundation (Slice S0)

**Input**: Design documents from `/specs/001-platform-foundation/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/serial_commands.md, contracts/internal_interfaces.md, quickstart.md

**Tests**: INCLUDED. The spec requires off-device tests (FR-024), and constitution Principle V requires
Unity tests for every `src/core/` module. They are written before the matching implementation.

**Organization**: Tasks are grouped by user story (spec.md US1–US5).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on incomplete tasks)
- **[Story]**: US1 navigation · US2 settings · US3 graceful boot · US4 diagnostics · US5 reproducible build

## Conventions (apply to every task)

- Run PlatformIO as `& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio …`
  (`pio.exe` is blocked by Windows Application Control on this machine). Written `pio` below.
- **Only `src/board/` may include LilyGoLib/SensorLib/XPowersLib headers or use board `#ifdef`s**
  (Principle II). `src/core/` may include only the C++ standard library (no Arduino, no LVGL).
- Only the UI task (Arduino `loop()`) may call LVGL. Other tasks use `EventBus`/`Snapshot`.
- All sizes in UI code go through `Theme::S(px)`. No hard-coded 502/410 coordinates.
- Interfaces are normative in `contracts/internal_interfaces.md`, serial syntax in
  `contracts/serial_commands.md`, entities and defaults in `data-model.md`.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: A reproducible build and the source skeleton. The output builds an empty firmware.

- [ ] T001 Create the directory skeleton `src/app/`, `src/board/`, `src/core/`, `src/services/`, `src/ui/`, `src/pages/`, `test/`, `tools/`, `partitions/`, `docs/adr/`, and move `src/main.cpp` to `src/app/main.cpp`, reduced to empty `setup()`/`loop()` that only `Serial.begin(115200)` and print "AeroDeck boot"
- [ ] T002 [P] Create `partitions/aerodeck_16MB.csv`, copied exactly from LilyGoLib's factory layout (research R6): nvs 0x9000/0x5000, otadata 0xe000/0x2000, app0 ota_0 0x10000/0x400000, app1 ota_1 0x410000/0x400000, ffat fat 0x810000/0x7E0000, coredump 0xFF0000/0x10000
- [ ] T003 [P] Create `tools/patch_lilygolib.py` (PlatformIO SCons script, `Import("env")`). For the current `$PIOENV`, locate `<PROJECT_LIBDEPS_DIR>/<PIOENV>/LilyGoLib/src/LilyGoDispInterface.cpp`. If it contains marker `AERODECK-PATCH-1`, print `patch_lilygolib: already patched`. Else replace the block starting `#if (ESP_ARDUINO_VERSION <= ESP_ARDUINO_VERSION_VAL(4,0,0))` through its matching outer `#endif` with: comment `// AERODECK-PATCH-1`, `#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)` → `rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB; data_endian=LCD_RGB_DATA_ENDIAN_LITTLE;` `#else` → `color_space=ESP_LCD_COLOR_SPACE_RGB;` `#endif` then `panel_config.vendor_config = nullptr;`, and print `patch_lilygolib: patched`. If neither the marker nor the original guard is found, call `env.Exit(1)` with a message naming the file. Must also work if the file is already patched by the earlier manual edit (detect `/* patched` or `Patched locally` text and rewrite to the canonical marked form)
- [ ] T004 [P] Create `tools/fw_version.py` (`pre:` script) that runs `git describe --always --dirty` (fallback `"dev"` if git is unavailable) and appends `CPPDEFINES` `AD_FW_VERSION=\"<value>\"`
- [ ] T005 Rewrite `platformio.ini`: `[platformio] default_envs = twatch_ultra`, `src_dir = src`, `boards_dir = ./boards`. In `[env_arduino]`: keep platform `espressif32@6.12.0`, monitor settings and RadioLib build flags; set `board_build.partitions = partitions/aerodeck_16MB.csv`; add `-std=gnu++17` and `build_unflags = -std=gnu++11`. Set `extra_scripts` as a **multi-line list, one script per indented line** (a space-separated single line is parsed as one invalid path):
  ```ini
  extra_scripts =
      pre:tools/fw_version.py
      post:tools/patch_lilygolib.py
  ```
  **Pin every dependency exactly (constitution VII: no `^`/`~`, Git URLs pinned to a commit)**. Use these values, which are the versions resolved in the last working build on 2026-11-11:
  ```ini
  ; [env_arduino] lib_deps
  xinyuan-lilygo/LilyGoLib @ 0.2.0
  lvgl/lvgl @ 9.6.0
  jgromes/RadioLib @ 7.8.1
  lewisxhe/XPowersLib @ 0.3.3
  lewisxhe/SensorLib @ 0.3.4
  mikalhart/TinyGPSPlus @ 1.1.0
  earlephilhower/ESP8266Audio @ 2.0.0
  h2zero/NimBLE-Arduino @ 2.5.2
  https://github.com/lewisxhe/ESP32-BLE-Keyboard-fork.git#20c8b9f1f3e01742c16110a6753ac95ee2b1c0ba
  https://github.com/lewisxhe/ESP32-BLE-Mouse-fork.git#61bcd326fff51efda7f20593fbf0993e72e70ff4
  ESP32 BLE Arduino                       ; framework-bundled, versioned by the platform pin
  ; [env:twatch_ultra] extra lib_deps
  https://github.com/lewisxhe/ST25R3916-fork.git#0c8e00f49d12881d1cff6d6f0879b90ce7ed4033
  https://github.com/lewisxhe/NFC-RFAL-fork.git#7bde4587ea44c36c178a43d083d7a7345c1e4cfe
  ; [env:twatchs3] extra lib_deps
  https://github.com/tonhuisman/IRremoteESP8266.git#e7d6b36a1d4a4caac32c0d32da971c68dcf864e3
  ```
  After the first build, look at the "Dependency Graph" in the build log. Confirm that the linked versions are the ones above, not LilyGoLib's own transitive duplicates (`lvgl@9.2.2`, `RadioLib@7.1.2`, `SensorLib@0.3.1`, `XPowersLib@0.2.9` are also installed in libdeps), and record the graph in `validation.md`. If a duplicate is linked, add `lib_ignore` for it or `lib_compat_mode = strict` and re-check. Keep `[env:twatch_ultra]` and `[env:twatchs3]` otherwise unchanged. **Remove** `[env:tlora_pager]`, `[env_emulator]`, all `emulator_*` envs and the commented upstream example `src_dir` lines. Add `[env:native]` with `platform = native` **pinned to the exact version that `pio` installs on first use** (run `pio pkg install -e native`, read the version, then write `platform = native@<x.y.z>`), `test_framework = unity`, `build_flags = -std=gnu++17 -I src`, `build_src_filter = +<core/>`, `test_build_src = yes`
- [ ] T006 Verify setup: `pio run -e twatch_ultra` succeeds and the log shows a `patch_lilygolib:` line. Record the output in the PR description

**Checkpoint**: The empty firmware builds through the automated patch.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: HAL, logging, event plumbing, theme, service task, page base class and a serial console framework. All user stories need these.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

### Tests (write first, must fail)

- [ ] T007 [P] Write Unity tests in `test/test_log_format/test_main.cpp` for `core::formatLogLine(char* out, size_t n, uint32_t ms, Lvl, const char* tag, const char* msg)`: produces `[1234][I][tag] msg`, truncates safely to `n` with a terminating NUL, and level filter `core::shouldLog(Lvl msg, Lvl threshold)` (E<W<I<D)
- [ ] T008 [P] Write Unity tests in `test/test_command_parser/test_main.cpp` for `core::CommandParser`: tokenizes `ad set bright 3` into argv; rejects lines without the `ad` prefix; reports `ERR unknown command`; matches registered commands by prefix tokens (`set get`, `set list`, `set` with 2 args); enforces 128-char line max and ≤8 tokens; dispatches to a registered handler with argc/argv

### Implementation

- [ ] T009 [P] Implement `src/core/LogFormat.h/.cpp` (`enum class Lvl : uint8_t {E,W,I,D}`, `formatLogLine`, `shouldLog`) so T007 passes
- [ ] T010 [P] Implement `src/core/CommandParser.h/.cpp` (fixed-size handler table of max 32 entries `{const char* pattern; uint8_t minArgs; Handler fn;}`, `Handler = void(*)(int argc, const char* const* argv, Reply&)`, where `Reply` is an abstract sink with `ok()`, `err(const char*)`, `line(const char*)`) so T008 passes
- [ ] T011 Run `pio test -e native` and confirm T007/T008 pass (install MinGW-w64 GCC first per quickstart Prerequisites if `gcc` is missing)
- [ ] T012 [P] Create `src/core/Types.h` (pure C++, `<cstdint>` only: enums `Cap`, `Rail`, `Periph`, `PeriphState`, structs `DisplayInfo`, `PmuReading`, `BootResult{bool pmuOk; bool safeMode; uint16_t failedMask;}`, plus `const char* periphName(Periph)`). Create `src/board/IBoard.h`, which includes `core/Types.h`, exactly per `contracts/internal_interfaces.md`, with `IBoard& board();`. There is **no** `src/board/Caps.h`: `core/` must never include anything from `board/` (Principle II)
- [ ] T013 [P] Create `src/board/I2cLock.h/.cpp`: a global FreeRTOS recursive mutex created on first use, RAII `I2cLock` guard (take with portMAX_DELAY, give in destructor)
- [ ] T014 Implement the minimal `src/board/BoardUltra.cpp` (guarded by `#ifdef ARDUINO_T_WATCH_S3_ULTRA`): `begin()` calls `instance.begin(0)`, `instance.setDisplayParams(true, false)` before it if required by LilyGoLib for DMA, then `beginLvglHelper(instance)`; `caps()` returns Lora|Haptics|Speaker|Mic|Nfc|Gnss|SdCard|ImuStepCounter|Amoled; `name()` = "T-Watch Ultra"; `display()` from `instance.width()/height()`; `setBrightnessPct()` maps 0–100 to `instance.setBrightness(pct*DEVICE_MAX_BRIGHTNESS_LEVEL/100)`; `loop()` calls `instance.loop()`; `readPmu()` reads battery %, battery current, die temperature and charging from `instance.pmu` under `I2cLock`. **Peripheral status (needed by US2's About/Status section):** keep the `uint32_t devices_probe` returned by `instance.begin()` and implement `periph(Periph)` by mapping `HW_RADIO_ONLINE`→Lora, `HW_NFC_ONLINE`→Nfc, `HW_GPS_ONLINE`→Gnss, `HW_SD_ONLINE`→SdCard, `HW_DRV_ONLINE`→Haptics, `HW_BHI260AP_ONLINE`→Imu, `HW_RTC_ONLINE`→Rtc, `HW_TOUCH_ONLINE`→Touch, `HW_EXPAND_ONLINE`→Expander, `HW_PMU_ONLINE`→Pmu: `Ok` if the bit is set; `Failed` if the board has the peripheral (caps) but the bit is clear; `Absent` if the board lacks it. Display = `Ok` once `beginLvglHelper` returns; Mic/Speaker = `Ok` (no probe bit). Signature `begin(simFailMask, safeMode, headless)` per contract, but in this task ignore all three arguments. Leave rails, button, simulated failures, the PMU pre-probe and headless for US1/US3 tasks (`setRail()` as a no-op with a TODO referencing T043)
- [ ] T015 [P] Implement `src/services/Log.h/.cpp`: `logf(Lvl, tag, fmt, ...)` using `core::formatLogLine` + `Serial`, with a runtime threshold (`Log::setLevel`) and a FreeRTOS mutex so lines from both cores never interleave. Add macros `LOGE/LOGW/LOGI/LOGD(tag, fmt, ...)`
- [ ] T016 [P] Implement `src/services/EventBus.h/.cpp` (event struct per data-model §8, `subscribe(depth)` returns a FreeRTOS queue, `publish()` posts non-blocking to every subscriber and counts drops; max 8 subscribers) and `src/services/Snapshot.h` (seqlock template: writer increments seq odd→write→even; reader retries while odd or changed)
- [ ] T017 [P] Implement `src/ui/Theme.h/.cpp`: `Theme::begin(DisplayInfo)` computes `scale = min(w,h)/240.0f`; `S(int px)`; palette tokens `bg, fg, hudGreen #00FF66, caution #FFB000, warning #FF3030, advisory #00C8FF, accent`; shared `lv_style_t` objects (screen, title label, body label, button, banner) initialised once; Montserrat font choice by scale (14/20/28 if enabled in LilyGoLib's `lv_conf.h`; check and use the largest available)
- [ ] T018 [P] Create `src/ui/Page.h` with the `Page` abstract class and `enum class PageId : uint8_t { Home, PlaceholderA, PlaceholderB, Settings, Count }` per contracts
- [ ] T019 Implement `src/services/SvcTask.h/.cpp`: a FreeRTOS task pinned to core 0, priority 2, 4096-word stack, 50 ms tick loop calling a registered list of `void(*)(uint32_t nowMs)` tick functions (max 8); `SvcTask::begin()`
- [ ] T020 Implement `src/services/SerialConsole.h/.cpp`: a tick function (registered on `SvcTask`) that reads `Serial` non-blocking into a 128-byte line buffer and dispatches complete lines through `core::CommandParser`. `Reply` writes `OK`/`ERR …`/data lines plus a terminating blank line. `SerialConsole::add(pattern, minArgs, fn)` lets each module register commands. Built-in commands: `ad help`, `ad info` (fw `AD_FW_VERSION`, `board().name()`, uptime, `esp_reset_reason()`, safeMode flag), `ad reboot`
- [ ] T021 Wire `src/app/main.cpp`: `setup()` → `Serial.begin` → `Log` → `board().begin(0, false, false)` → `Theme::begin(board().display())` → `SvcTask::begin()` (with `SerialConsole` tick) → create a temporary label "AeroDeck" on `lv_screen_active()`. `loop()` → `board().loop()` → `lv_timer_handler()` → `delay(1)`
- [ ] T022 Build and flash (`pio run -e twatch_ultra -t upload --upload-port COM3`). Verify the label shows and `ad info` answers over serial

**Checkpoint**: Foundation ready. The watch boots, shows a label and answers serial commands.

---

## Phase 3: User Story 1 - Boot and navigate between cockpit pages (Priority: P1) 🎯 MVP

**Goal**: Wrapping page ring (Home → A → B → Settings), swipe navigation, button-to-home / back-to-last, page indicator.

**Independent Test**: quickstart V2 + V3. Boot ≤3 s ×10; swipe sequence and wrap; button semantics; 100 swipes with no skip.

### Tests for User Story 1

- [ ] T023 [P] [US1] Write Unity tests in `test/test_nav/test_main.cpp` for `core::NavModel` (ring size N, home index 0): `next()/prev()` wrap; `button()` from non-home → home and remembers `lastNonHome`; `button()` on home → `lastNonHome` (initially index 1); while `busy` is set, `next/prev` are ignored and `button()` sets `pendingHome`; `transitionDone()` clears busy and returns home as the next target if pendingHome was set

### Implementation for User Story 1

- [ ] T024 [US1] Implement `src/core/NavModel.h/.cpp` (pure, returns the target index or -1 for "no change") so T023 passes
- [ ] T025 [US1] Implement `src/ui/PageManager.h/.cpp` per contracts. `registerPage()` in ring order. Lazily create one `lv_obj_create(NULL)` screen per page, style it with the Theme screen style, call `onCreate`. Add an `LV_EVENT_GESTURE` callback on every screen: `lv_indev_get_gesture_dir(lv_indev_active())` LEFT → `next()`, RIGHT → `prev()`, ignore TOP/BOTTOM. Navigate with `lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_LEFT/RIGHT, 200, 0, false)`. Mark busy until `LV_EVENT_SCREEN_LOADED`, then apply pending. Call `onExit` on the old page and `onEnter` on the new one. Publish `PageChanged`. `onTick` for the visible page at 10 Hz via an `lv_timer`
- [ ] T026 [P] [US1] Implement `src/ui/PageIndicator.h/.cpp`: on `lv_layer_top()`, a row of N dots (current dot filled, `S(8)` px, bottom-centre) plus the page title. Shown on each page change and hidden by a one-shot `lv_timer` after 1000 ms
- [ ] T027 [P] [US1] Implement `src/pages/HomePage.h/.cpp`: placeholder with title "AERODECK" in `hudGreen` and an uptime clock `HH:MM:SS` (from `millis()`, label updated in `onTick`), centred via Theme
- [ ] T028 [P] [US1] Implement `src/pages/PlaceholderPage.h/.cpp`: constructor `(PageId, const char* title)`, shows the title large and "Coming in a later slice" small
- [ ] T029 [US1] Add button support to `src/board/BoardUltra.cpp`: `onButtonClick(cb, ctx)` stores the callback; in `begin()` register `instance.onEvent(...)` (LilyGoEventManage API) for `POWER_EVENT`; in the handler, if `instance.getPMUEventType(params) == PMU_EVENT_KEY_CLICKED`, invoke the callback (it runs on the UI task because `instance.loop()` runs in `loop()`). Check the exact LilyGoEventManage registration method name in `.pio/libdeps/twatch_ultra/LilyGoLib/src/LilyGoEventManage.h` and use it
- [ ] T030 [US1] Wire US1 in `src/app/main.cpp`: create HomePage, PlaceholderPage(A,"PAGE A"), PlaceholderPage(B,"PAGE B") and a temporary PlaceholderPage(Settings,"SETTINGS") (replaced in T038). Register them in that order, `PageManager::begin()`, and `board().onButtonClick` → `PageManager::buttonPressed()`. Remove the temporary label from T021
- [ ] T031 [US1] Add timing instrumentation (INFO-level logs, tag `nav`): (a) in `src/app/main.cpp`, `boot→home <ms>` = `esp_timer_get_time()/1000` (counts from chip reset) when Home's first `LV_EVENT_SCREEN_LOADED` fires (SC-001); (b) in `src/ui/PageManager.cpp`, record `millis()` when a navigation request is accepted (swipe gesture or button) and log `nav <from>→<to> <kind:swipe|button> <ms>` on the matching `LV_EVENT_SCREEN_LOADED`. Button latency = button event → loaded (SC-003, ≤500 ms). Transition time = swipe → loaded (SC-006, ≤300 ms)
- [ ] T032 [US1] Flash and run quickstart V2 and V3 (without the overlay FPS check). Fix any issue. In `specs/001-platform-foundation/validation.md` record the 10 cold-boot times (SC-001), max/avg button→home latency over 20 presses from random pages (SC-003), and max/avg swipe transition time over the 100-swipe run (SC-006), each with a pass/fail against its threshold

**Checkpoint**: The MVP navigation shell works on the watch.

---

## Phase 4: User Story 2 - Settings that survive restarts (Priority: P1)

**Goal**: NVS-backed, validated, schema-versioned settings; Settings page with brightness, overlay toggle, log level, restore defaults (with confirmation) and an About/Status section.

**Independent Test**: quickstart V4 (persistence over 10 power cycles, including 3 cut during save; range rejection; restore defaults with cancel/confirm).

### Tests for User Story 2

- [ ] T033 [P] [US2] Write Unity tests in `test/test_settings/test_main.cpp` for `core::SettingDefs`: the registry contains the data-model §1 keys with correct types/defaults/ranges; `validate(key, value)` rejects out-of-range values and accepts in-range ones; `sanitize(key, storedValue, present)` returns the default for missing/out-of-range values; `schemaAction(stored, current, present)` returns `FirstBoot | Load | Migrate | ResetNewer`; secret flag lookup; key names ≤15 chars (static check across the table)

### Implementation for User Story 2

- [ ] T034 [US2] Implement `src/core/SettingDefs.h/.cpp` (`enum class SettingKey`, constexpr table of `SettingDef{key name, type, def, min, max, secret, editable}`, the functions from T033, `kSchemaVersion = 1`) so T033 passes
- [ ] T035 [US2] Implement `src/services/SettingsStore.h/.cpp` over Arduino `Preferences` (namespace `aerodeck`). `begin()` runs the schema state machine from data-model §1; on NVS open failure, use in-memory defaults and remember `storageError` (US3 turns it into a caution). It loads all values into a RAM cache with `sanitize`. `set()` validates, writes the single key, updates the cache and publishes `SettingsChanged`. `restoreDefaults()` = `clear()` + write schema + defaults. Writes are executed on the `svc` task through a small request queue so the UI never blocks on flash. Logs key names only, never secret values
- [ ] T036 [US2] Register serial commands in `src/services/SettingsStore.cpp` via `SerialConsole::add`: `ad set list`, `ad set get <key>`, `ad set <key> <value>`, `ad set reset`, exactly per `contracts/serial_commands.md` (secret values printed as `***`)
- [ ] T037 [US2] Apply settings live in `src/app/main.cpp`: on boot and on `SettingsChanged`, `bright` → `board().setBrightnessPct((bright+1)*20)`; `loglvl` → `Log::setLevel`. Drain the UI event queue in `loop()` for this
- [ ] T038 [US2] Implement `src/pages/SettingsPage.h/.cpp`: a scrollable flex column with: Brightness (5-step `lv_slider` or −/+ buttons ≥ `S(48)` touch size), Diagnostics overlay (`lv_switch`), Log level (`lv_dropdown` ERROR/WARN/INFO/DEBUG), "Restore defaults" button → `lv_msgbox` with Cancel/Confirm, and an About/Status section (firmware version, board name, one row per `Periph` showing `board().periph(p)` as OK/FAILED/ABSENT/SIMFAIL, coloured with Theme tokens). Controls call `SettingsStore::set`, and the page refreshes from the cache on `onEnter` and on `SettingsChanged`. Replace the temporary Settings placeholder in `src/app/main.cpp`
- [ ] T039 [US2] Flash and run quickstart V4, including 3 power-cut-during-save trials. Record the results in `specs/001-platform-foundation/validation.md`

**Checkpoint**: US1 + US2 work. Settings persist.

---

## Phase 5: User Story 3 - Graceful boot when hardware misbehaves (Priority: P1)

**Goal**: Optional-peripheral failures become cautions; simulated failures; PMU-absent handling without a loop; safe mode after repeated fast failures; optional rails off; watchdog.

**Independent Test**: quickstart V5 and V7 (simulate each failure → Home ≤3 s with a named caution; 1 h run with no reset; rails off after boot).

### Tests for User Story 3

- [ ] T040 [P] [US3] Write Unity tests in `test/test_boot_guard/test_main.cpp` for `core::BootGuard`: `onBoot(record)` with an invalid magic initialises the record; increments `fastFailCount` when the previous boot didn't reach stable; `safeMode` becomes true at count ≥3; `onStable(record)` clears the count; `onBoot` with `lastStage == DisplayInit` and count ≥3 returns `BootMode::Headless` (U1, see T047), while other stages at count ≥3 return `BootMode::Safe`; and `core::SimFail`: bit ↔ `core::Periph` (from `core/Types.h`) and name ↔ bit (`"lora"` → bit0 …) per data-model §3, plus the rejection of core peripherals. Mapping to LilyGoLib `NO_HW_*` flags is **not** tested here; it lives in `src/board/` (T042)

### Implementation for User Story 3

- [ ] T041 [US3] Implement `src/core/BootGuard.h/.cpp` (record struct per data-model §6 including `lastStage`, `enum class BootMode {Normal, Safe, Headless}`, pure functions) and `src/core/SimFail.h/.cpp` (bit ↔ `core::Periph` mapping, name ↔ bit for the serial command; includes only `core/Types.h`) so T040 passes
- [ ] T042 [US3] Extend `src/board/BoardUltra.cpp` `begin(simFailMask, safeMode, headless)`: (1) `Wire.begin(SDA,SCL)` and probe address 0x34. If there's no ACK, return `pmuOk=false` without calling `instance.begin`. (2) Translate `simFailMask` (via `core::SimFail`) to LilyGoLib `NO_HW_LORA|NO_HW_NFC|NO_HW_GPS|NO_HW_SD|NO_HW_MIC|NO_HW_DRV|NO_HW_SENSOR|NO_HW_RTC`. In safe mode set all of them. This board-side mapping table stays in `src/board/`. (3) Call `instance.begin(flags)`. (4) If `headless`, skip `beginLvglHelper` and report Display as `Failed`. (5) Extend the T014 `periph()` mapping: a peripheral whose bit was simulated reports `SimulatedFail` instead of `Failed`; with `NO_HW_MIC` set, Mic/Speaker report `SimulatedFail`
- [ ] T043 [US3] Implement rail control in `src/board/BoardUltra.cpp` `setRail(Rail,on)` using `instance.powerControl(PowerCtrlChannel, on)` (check the `PowerCtrlChannel` enum in LilyGoLib `LilyGoTypedef.h` for the LoRa/GPS/NFC/speaker channels; map Display and Sensor too). Add a wrapper LVGL touch `read_cb` that takes `I2cLock` around LilyGoLib's original indev read callback (get it with `lv_indev_get_read_cb` on the default indev after `beginLvglHelper`, store it, and replace it)
- [ ] T044 [P] [US3] Implement `src/services/Cautions.h/.cpp` per data-model §4: a fixed array of active cautions (max 16), `raise/clear/active`, publishing `CautionChanged`
- [ ] T045 [P] [US3] Implement `src/ui/CautionBanner.h/.cpp`: an amber banner on `lv_layer_top()` (top, full width, height `S(28)`, black text). Hidden when there are no cautions, and it cycles through active cautions every 3 s; it updates on `CautionChanged`
- [ ] T046 [US3] Implement `src/services/PowerService.h/.cpp`: per-rail refcounts guarded by a mutex; `RailLease` RAII (move-only) calling `board().setRail` on 0↔1 transitions; `begin()` forces Lora/AudioAmp/Gnss/Nfc rails OFF and logs each; a 1 Hz `svc` tick that reads `board().readPmu()` into `Snapshot<PowerSnapshot>`
- [ ] T047 [US3] Boot orchestration in `src/app/main.cpp`: an `RTC_NOINIT_ATTR` boot record → `BootGuard::onBoot` (count resets and panics/WDT resets as fast fails using `esp_reset_reason()`); a one-shot 10 s timer → `onStable` (sets `lastStage = Stable`). Write the `lastStage` breadcrumb **before** each stage: `PmuProbe` → `VendorInit` (`instance.begin`) → `DisplayInit` (`beginLvglHelper` + Theme) → `UiInit` (pages). `BoardUltra::begin` gets a `bool headless` parameter (extend `IBoard::begin` and the contract) that skips `beginLvglHelper` and all LVGL calls. In `BootMode::Headless`: start only `SvcTask` + `SerialConsole`, log `FATAL: display init failed 3x — headless mode, use 'ad reboot' or 'ad set reset'` every 5 s, and never call LVGL or reset. Settings `simfail` → `board().begin(mask, safeMode, headless)`. If `pmuOk == false`: log FATAL every 5 s in an idle loop (serial console still running), never return or reset. To verify, temporarily force an `abort()` right after setting `DisplayInit` (behind `-D AD_TEST_DISPLAY_CRASH`), flash, and confirm 3 resets then headless with no further resets. Record this in `validation.md` and remove the flag. For each `Periph` with state Failed/SimulatedFail raise the caution `"<NAME> FAIL"`, plus `SAFE MODE`, `SETTINGS RESET`, `SETTINGS STORAGE ERROR` when applicable. Start `PowerService`
- [ ] T048 [US3] Register serial commands in `src/app/main.cpp` (or a new `src/services/BoardCommands.cpp`): `ad periph` and `ad simfail <name|none> ...` (writes the `simfail` setting, reply `OK mask=0x..`) per contract
- [ ] T049 [US3] Enable the task watchdog: subscribe the `svc` task (`esp_task_wdt_add(NULL)` + reset each tick) and the Arduino loop task (`enableLoopWDT()`) with a 5 s timeout, in `src/services/SvcTask.cpp` and `src/app/main.cpp`
- [ ] T050 [US3] Flash and run quickstart V5 (each peripheral alone, then all together, plus the 1 h run) and V7. Record the results in `specs/001-platform-foundation/validation.md`

**Checkpoint**: US1–US3 work. The firmware is now constitution-compliant for Principle I.

---

## Phase 6: User Story 4 - Developer diagnostics (Priority: P2)

**Goal**: FPS/heap overlay, runtime log level, `ad log`, `ad stat`.

**Independent Test**: quickstart V6.

### Implementation for User Story 4

- [ ] T051 [P] [US4] Implement `src/ui/DebugOverlay.h/.cpp`: a small label on `lv_layer_top()` (bottom-left, semi-transparent black bg) updated by a 1 s `lv_timer`. FPS = number of `LV_EVENT_RENDER_READY` display events per second (add the event cb on `lv_display_get_default()`). Heap = `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`, min = `heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)`, psram = `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)`. Values are also stored in a `Snapshot<DiagSnapshot>`. Visibility follows the `overlay` setting
- [ ] T052 [US4] Register `ad log <error|warn|info|debug>` (persists `loglvl`) and `ad stat` (from `DiagSnapshot`) in `src/ui/DebugOverlay.cpp` / `src/services/Log.cpp` per contract
- [ ] T053 [US4] Audit for secrets: check that `SettingsStore`, `SerialConsole` and `Log` never format a value whose `SettingDef.secret` is true. Add a Unity test case in `test/test_settings/test_main.cpp` that marks a test-only secret def and asserts the `formatSettingForOutput()` helper (add it to `src/core/SettingDefs.cpp`) returns `***`
- [ ] T054 [US4] Flash and run quickstart V6, plus the SC-006/SC-007 checks (≥30 FPS during swipes, heapmin ≥40 KB after 30 min). If heap < 40 KB, switch `BoardUltra` to PSRAM draw buffers (`setDisplayParams(false, …)`) and re-measure (research R7). Record the results in `specs/001-platform-foundation/validation.md`

**Checkpoint**: Measurement tooling is in place for every later slice.

---

## Phase 7: User Story 5 - Reproducible build from a clean checkout (Priority: P2)

**Goal**: Clean-checkout Ultra build, S3 compile, native tests, documented decisions.

**Independent Test**: quickstart V1.

### Implementation for User Story 5

- [ ] T055 [P] [US5] Implement `src/board/BoardS3.cpp` (guarded by `#ifdef ARDUINO_T_WATCH_S3`) using the T-Watch S3 LilyGoLib `instance`: same `IBoard` behaviour as Ultra where the API exists; `caps()` = Lora|Haptics|Speaker|Mic|IrTx|ImuStepCounter; peripherals the S3 lacks report `Absent`; PMU-key click → button callback. It must compile. On-device S3 testing is out of scope
- [ ] T056 [US5] Ensure `pio run -e twatchs3` compiles (fix any Ultra-only symbol leaking outside `src/board/`)
- [ ] T057 [P] [US5] Write `docs/adr/008-lilygolib-patch-post-script.md` (context: IDF-4.4 guard bug; decision: `post:` script; evidence: `builder/main.py:167/173/180`, `piolib.py:1204`; consequences; fallback: vendor or fork)
- [ ] T058 [P] [US5] Write `docs/adr/009-fat-instead-of-littlefs.md` (LilyGoLib mounts FFat and exposes USB MSC; assets go on FAT; consequences for S5)
- [ ] T059 [US5] Amend `docs/software_architecture_specification.md`: §7.1 (FAT `ffat`, `partitions/aerodeck_16MB.csv`), §9 (`post:` patch script + `pre:` version script, `native` env with `build_src_filter`), and add ADR-008/ADR-009 rows to §11
- [ ] T060 [US5] Clean-checkout verification: delete `.pio`, then run `pio run -e twatch_ultra` (expect "patched"), again (expect "already patched"), `pio run -e twatchs3`, `pio test -e native`. Record the results in `specs/001-platform-foundation/validation.md`

**Checkpoint**: Every user story is complete.

---

## Phase 8: Polish & Cross-Cutting Concerns

- [ ] T061 [P] Replace the upstream `README.md` with an AeroDeck README: purpose, hardware, how to build/flash (including the `python -m platformio` workaround and the MinGW prerequisite), `ad` serial commands, links to `docs/`
- [ ] T062 [P] Check layering with `grep`: no `#include <LilyGo`, `SensorLib`, `XPowers`, `RadioLib`, `ARDUINO_T_WATCH` outside `src/board/`; no `lv_` calls in `src/services/` or `src/core/`; no `502`/`410` literals outside `src/board/`. Fix violations
- [ ] T063 Run the full quickstart (V1–V7) end to end on the final build, complete `specs/001-platform-foundation/validation.md` with every SC-001…SC-009 result, and tick the merge gates from the constitution (both builds, native tests, on-device checks, ≥30 FPS/≥40 KB, no secrets)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: none. T005 depends on T002–T004; T006 depends on T005.
- **Foundational (Phase 2)**: depends on Setup. T009←T007, T010←T008, T011←T009/T010, T014←T012/T013, T020←T010/T015/T019, T021←T014–T020, T022←T021.
- **US1 (Phase 3)**: depends on Foundational. T024←T023; T025←T018/T024; T030←T025–T029; T031←T030; T032←T031.
- **US2 (Phase 4)**: depends on Foundational. Its Settings page needs US1's PageManager (T025) to be visible. The settings store (T033–T037) can be built in parallel with US1. Real peripheral status for the About/Status section comes from Foundational T014, so US2 doesn't depend on US3 (US3 only adds `SimulatedFail`).
- **US3 (Phase 5)**: depends on Foundational and US2 (simfail is a setting, cautions are listed on the Settings page). T041←T040; T047←T041–T046.
- **US4 (Phase 6)**: depends on Foundational. T051 is independent of other stories; T052–T053 depend on US2 (settings).
- **US5 (Phase 7)**: T055–T059 can start after Foundational. T060 should run after all other stories so the full code base is verified clean.
- **Polish (Phase 8)**: after all stories.

### User Story Dependencies

```
Setup → Foundational ─┬─► US1 (nav) ─────────┐
                      ├─► US2 (settings) ◄───┤ (page needs PageManager)
                      │      └─► US3 (graceful boot)
                      ├─► US4 (diagnostics; T052+ after US2)
                      └─► US5 (S3 board, ADRs; T060 last)
```

### Parallel Opportunities

- Setup: T002, T003, T004 together.
- Foundational: T007+T008 (tests), then T009+T010; T012, T013, T015, T016, T017, T018 all in parallel.
- US1: T026, T027, T028 in parallel after T025 starts (separate files).
- US2: T033→T034→T035 can proceed while US1's T025–T030 are being built.
- US3: T044 and T045 in parallel with T042/T043.
- US5: T055, T057, T058 in parallel with any other story.

---

## Parallel Example: Foundational

```text
Task: "T012 Create src/core/Types.h and src/board/IBoard.h"
Task: "T013 Create src/board/I2cLock.h/.cpp"
Task: "T015 Implement src/services/Log.h/.cpp"
Task: "T016 Implement src/services/EventBus.h/.cpp and Snapshot.h"
Task: "T017 Implement src/ui/Theme.h/.cpp"
Task: "T018 Create src/ui/Page.h"
```

## Parallel Example: User Story 1

```text
Task: "T026 [US1] Implement src/ui/PageIndicator.h/.cpp"
Task: "T027 [US1] Implement src/pages/HomePage.h/.cpp"
Task: "T028 [US1] Implement src/pages/PlaceholderPage.h/.cpp"
```

---

## Implementation Strategy

### MVP First (User Story 1 only)

1. Phase 1 Setup → Phase 2 Foundational → Phase 3 US1.
2. **Stop and validate** with quickstart V2/V3 on the watch. This is the first wearable shell.

### Incremental Delivery

1. US1 navigation (MVP) → US2 settings → US3 graceful boot. These complete the P1 stories, and the constitution's Principle I is satisfied.
2. US4 diagnostics → US5 portability/ADRs → Polish. This closes milestone M0.
3. Commit after each task or logical group, and flash at each checkpoint.

---

## Notes

- `validation.md` is a running record of on-device results. It is created at T032 and completed at T063.
- If a LilyGoLib API name in a task differs from the installed source, follow the source and note
  the difference in `validation.md`. Never patch LilyGoLib beyond `tools/patch_lilygolib.py`.
- Never call `assert`/`abort` for optional hardware. Return an error and raise a caution.
