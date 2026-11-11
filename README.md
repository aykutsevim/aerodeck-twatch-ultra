# AeroDeck: glass-cockpit firmware for the LilyGO T-Watch Ultra

AeroDeck turns the LilyGO **T-Watch Ultra** (ESP32-S3, 410×502 portrait AMOLED with 64 px rounded corners) into an aviation-style watch:
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

**Current state:** slice S1. The home page is the **Primary Flight Display**:
- an artificial horizon driven by the BHI260AP's on-chip fusion, with LOCK when level;
- hour and minute tapes, plus a heading tape that turns the seconds into compass degrees (1 s = 6°);
- ZULU (UTC) and LOCAL time;
- an EICAS strip: FUEL QTY (battery %), FUEL FLOW (drain %/h), EGT (power-chip °C) and a CHG annunciator;
- a LOW FUEL caution.

It sits on the S0 platform: page ring, settings, cautions, diagnostics and boot-loop protection.

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

## Using the watch

- **First use: connect Wi-Fi** so the watch sets its own time. Go to Settings → **Set up Wi-Fi**. The watch shows a QR
  code and a one-time code; scan it with your phone camera (or join `AeroDeck-XXXX` with the code). The setup page
  opens; pick your network, enter its password and Save. The watch then syncs UTC at boot and every hour (Wi-Fi is
  off between syncs). Developers can also use `ad wifi set "<ssid>" "<password>"` over USB.
- **Time zone:** Settings → Time zone (named zones with summer time; default Istanbul). ZULU always shows UTC.
- **No network?** `ad time set 2026-11-11T09:24:00Z` (UTC) still works as a fallback.
- **Weather (WX page):** swipe right from the watch face. It shows the latest METAR for your station (default LTAC):
  wind (with a compass), visibility, QNH, temperature/dewpoint, clouds, the raw report, and the flight category
  (VFR green, MVFR blue, IFR red, LIFR magenta). It updates with every time sync (same Wi-Fi session) and when you tap
  **REFRESH**. **STALE** means the report is more than 90 min old; NO WIFI / NET FAIL / NO REPORT / SRV ERROR say why
  the last fetch failed (the last good report stays). Change the station in Settings → Weather station (four letter
  wheels) or with `ad wx set LTFM`.
- **Alerts (master annunciator):** every problem shows in one band at the top of every page: **WARNING** red,
  **CAUTION** amber, **ADVISORY** cyan, with `+N` when more are active. It flashes until you **tap it** (tap =
  acknowledge all: repeating vibration stops), then cycles through the active alerts every 3 s.
- **Low battery:** below 15 % (not charging) → **STALL** warning with a stick-shaker vibration every 2 s until
  acknowledged; at 5 % → **PULL UP**. Charging (or recovery past 17 % / 7 %) clears them.
- **Vibration:** Settings → ALERTS → Vibration (Off / Low / High; warnings always vibrate) and **Quiet mode** (no
  caution/advisory vibration, no sound from S5). Page changes and controls give a short click. **Test alert** raises
  a 10 s CAUTION so you can feel it.
- **Swipe left/right** to move through the page ring: PFD → WX → Page B → Settings → PFD.
- **Side button (short press):** return to Home. Press again on Home to go back to the last page.
- **Settings page:** brightness, diagnostics overlay (FPS/heap), log level, local offset (30-min steps),
  restore defaults, and firmware/peripheral status.
- **Horizon flags:** ATT ALIGN for the first ~3 s after boot (motion sensor starting), ATT FAIL if motion sensing
  is unavailable (the clock and gauges keep working).
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
| `ad time` · `ad time set <YYYY-MM-DDTHH:MM:SSZ>` | Show / set the UTC clock (kept in the RTC; offline fallback) |
| `ad wifi` · `ad wifi set "<ssid>" "<psk>"` · `ad wifi forget` · `ad wifi test` · `ad wifi scan` | Wi-Fi status and credentials (the password is never printed) |
| `ad wifi setup` · `ad wifi setup stop` | Start/stop the phone setup hotspot (the code is shown on the watch only) |
| `ad sync` | Sync time now |
| `ad tz` · `ad tz list [filter]` · `ad tz set <name\|index>` | Time zone |
| `ad wx` · `ad wx fields` · `ad wx fetch` | Weather state, decoded fields, fetch now |
| `ad wx set <ICAO>` | Weather station (4 letters) |
| `ad wx decode "<METAR>" [show]` · `ad wx clear` · `ad wx age <s>` | Test hooks: decode text (optionally show it on the WX page), clear the cache, pretend the report is N s old |
| `ad alert` · `ad alert raise <id> [W\|C\|A] ["text"]` · `ad alert clear <id>` · `ad alert ack` | List / raise / clear / acknowledge alerts (ids: `stall`, `pullup`, `test1..3`, …) |
| `ad haptic <click\|advisory\|caution\|shaker\|stop>` | Play a vibration pattern (current strength / quiet rules) |
| `ad fuel sim <pct\|off> [chg\|nochg]` | Test hook: simulate the battery level (and charging) |
| `ad wifi age <s>` | Test hook: pretend the last sync was N s ago (TIME UNSYNC) |
| `ad att` | Attitude: status, pitch, roll, LOCK, sample rate, latency, raw gravity |
| `ad att sweep <on\|off>` | Test hook: synthetic horizon motion for FPS measurement |
| `ad fuel` · `ad fuel sim <pct\|off>` | EICAS values · test hook overriding battery % (LOW FUEL testing) |
| `ad reboot` | Restart |

Full contracts: [S0](specs/001-platform-foundation/contracts/serial_commands.md) ·
[S1 additions](specs/002-pfd-watch-face/contracts/serial_commands.md) ·
[S2 additions](specs/003-time-sync/contracts/serial_commands.md) ·
[S3 additions](specs/004-metar-weather/contracts/serial_commands.md) ·
[S4 additions](specs/005-alert-haptics/contracts/serial_commands.md).

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

## Fonts

HUD digits use **DSEG7 Classic Bold** by keshikan (SIL Open Font License 1.1, `src/ui/fonts/OFL.txt`), converted
with `lv_font_conv`; see [`tools/gen_fonts.md`](tools/gen_fonts.md).

## Credits

Built on [LilyGoLib](https://github.com/Xinyuan-LilyGO/LilyGoLib) and the
[LilyGoLib-PlatformIO](https://github.com/Xinyuan-LilyGO/LilyGoLib-PlatformIO) template by LILYGO,
[LVGL](https://lvgl.io/), RadioLib, SensorLib and XPowersLib. Board definitions in `boards/` and
variants in `variants/` come from the LILYGO template, and `lib/libhelix-mp3` keeps its own licences.
