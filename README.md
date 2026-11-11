# AeroDeck: glass-cockpit firmware for the LilyGO T-Watch Ultra

AeroDeck turns the LilyGO **T-Watch Ultra** (ESP32-S3, 502×410 AMOLED) into an aviation-style watch:
a Primary Flight Display watch face and swipeable Multi-Function Display pages, each putting one onboard
peripheral to work (IMU horizon, PMU "engine" gauges, LoRa TCAS radar, haptic stick shaker, GPWS voice…).
The T-Watch S3 is a secondary, compile-only target.

Development follows [spec-kit](https://github.com/github/spec-kit) in feature slices. See:

| Document | Purpose |
| :--- | :--- |
| [`docs/software_requirements_specification.md`](docs/software_requirements_specification.md) | What the firmware must do (AD-SRS-001) |
| [`docs/software_architecture_specification.md`](docs/software_architecture_specification.md) | How it is built (AD-SAS-001) |
| [`docs/product_roadmap.md`](docs/product_roadmap.md) | Slices S0–S13 and milestones |
| [`docs/adr/`](docs/adr/) | Architecture decision records |
| [`.specify/memory/constitution.md`](.specify/memory/constitution.md) | Non-negotiable project principles |
| [`specs/`](specs/) | Per-slice spec, plan, tasks and validation records |

**Current state:** slice S0, platform foundation. It has a page ring, settings, cautions, diagnostics
and boot-loop protection. Home is a placeholder until S1 (PFD watch face).

## Build & flash

Prerequisites: [PlatformIO Core 6.x](https://platformio.org/). Everything else (toolchain, pinned libraries,
the LilyGoLib compatibility patch) is installed or applied automatically by the build.

```powershell
pio run -e twatch_ultra                                  # build (first build downloads dependencies)
pio run -e twatch_ultra -t upload --upload-port COM3     # flash
pio device monitor -p COM3 -b 115200                     # serial console
pio run -e twatchs3                                      # secondary target must also compile
```

> **Windows with Application Control:** if `pio.exe` is blocked, run PlatformIO through its Python:
> `& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio …`
>
> **Windows path length:** some dependencies (RadioLib examples) have very long file paths. Clone into a
> short directory (e.g. `D:\work\aerodeck`) or enable Windows long-path support, otherwise the first
> dependency install fails with "No such file or directory".

### Host unit tests

Pure logic in `src/core/` has Unity tests:

```powershell
pio test -e native
```

This needs a host C++ compiler on `PATH`. On Windows install MinGW-w64, e.g. MSYS2 then
`pacman -S mingw-w64-ucrt-x86_64-gcc` and add `C:\msys64\ucrt64\bin` to `PATH`. Check with `gcc --version`.

## Using the watch (S0)

- **Swipe left/right** to move through the page ring: Home → Page A → Page B → Settings → Home.
- **Side button (short press):** return to Home. Press again on Home to go back to the last page.
- **Settings page:** brightness, diagnostics overlay (FPS/heap), log level, restore defaults, and
  firmware/peripheral status.
- An **amber banner** names any peripheral that failed to start. The watch keeps working without it.

## Serial console (`ad …`)

115200 baud, one command per line. Replies start with `OK` or `ERR <reason>` and end with a blank line.

| Command | Purpose |
| :--- | :--- |
| `ad help` | List commands |
| `ad info` | Firmware version, board, uptime, reset reason, safe-mode flag |
| `ad periph` | Peripheral status (OK / FAILED / ABSENT / SIMFAIL / PENDING) |
| `ad set list` · `ad set get <key>` · `ad set <key> <value>` · `ad set reset` | Settings |
| `ad simfail <lora\|nfc\|gnss\|sd\|mic\|haptics\|imu\|rtc\|none> …` | Simulate peripheral failures on next boot |
| `ad log <error\|warn\|info\|debug>` | Diagnostic level (persisted) |
| `ad stat` | FPS and free heap/PSRAM |
| `ad nav <next\|prev\|button>` | Test hook: navigate as if swiping or pressing the button |
| `ad reboot` | Restart |

Full contract: [`specs/001-platform-foundation/contracts/serial_commands.md`](specs/001-platform-foundation/contracts/serial_commands.md).

## Project layout

```text
src/app/       entry point and boot orchestration
src/board/     the only code that touches LilyGoLib / drivers (IBoard HAL)
src/core/      pure C++ logic, host-tested
src/services/  settings, power, logging, events, serial console (core 0)
src/ui/        page manager, theme, overlays (UI task only)
src/pages/     pages
test/          Unity tests for src/core/
tools/         build scripts (LilyGoLib patch, firmware version)
partitions/    project-owned partition table
```

## Credits

Built on [LilyGoLib](https://github.com/Xinyuan-LilyGO/LilyGoLib) and the
[LilyGoLib-PlatformIO](https://github.com/Xinyuan-LilyGO/LilyGoLib-PlatformIO) template by LILYGO,
[LVGL](https://lvgl.io/), RadioLib, SensorLib and XPowersLib. Board definitions in `boards/` and
variants in `variants/` come from the LILYGO template, and `lib/libhelix-mp3` keeps its own licences.
