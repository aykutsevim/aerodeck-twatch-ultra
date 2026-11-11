# AeroDeck — Software Architecture Specification (SAS)

| Field | Value |
| :--- | :--- |
| Document ID | AD-SAS-001 |
| Version | 1.0 (initial canonical baseline) |
| Date | 2026-11-10 |
| Status | Baseline for spec-kit development |
| Implements | AD-SRS-001 (`software_requirements_specification.md`) |

---

## 1. Purpose & scope
This document is the canonical statement of **how** AeroDeck is built: layers, modules, tasks, data flow, memory, power, protocols, build and test. Each spec-kit `/speckit.plan` run must follow it. A plan that deviates must record an ADR (§11).

## 2. Architectural drivers
| Driver | Source | Architectural response |
| :--- | :--- | :--- |
| Smooth horizon (≥30 FPS) | NFR-PERF-001/002/003 | UI task on its own core. Services never call LVGL. Non-blocking I/O on core 0 |
| Battery life | NFR-PWR-001/002 | Reference-counted `PowerRail` leases. Services take hardware only while active |
| Two boards with different hardware | §1.3 SRS, NFR-MNT-001/005 | `Board` HAL with capability flags. Features check capabilities, never `#ifdef`, in app code |
| Many independent features | Slice roadmap | Page + Service plug-in model with lifecycle hooks. Event bus decouples producers and consumers |
| Robust boot | NFR-REL-001 | Every service starts as fallible. Failure becomes a CAUTION, not an abort |
| Testability | NFR-MNT-004 | Pure-logic modules with no Arduino dependency, unit-tested on `env:native` |

## 3. Layered view

```
┌───────────────────────────────────────────────────────────────────┐
│ UI LAYER (core 1, UI task only — the only code that touches LVGL) │
│  PageManager · Theme (normal/NVG) · Widgets (Tape, Horizon,       │
│  Gauge, Annunciator, GuardedSwitch, PPI) · Pages (PFD, WX, REC,   │
│  CHR, TCAS, PTT, IFF, GPS, SETTINGS)                              │
├──────────────────────────── EventBus / Snapshots ─────────────────┤
│ SERVICE LAYER (core 0 tasks + cooperative ticks)                  │
│  PowerService · TimeService · NetService · WeatherService ·       │
│  AlertManager · HapticService · AudioService · AttitudeService ·  │
│  RecorderService · ChronoService · RadioService/TcasService ·     │
│  MicService · IffService · GnssService · SettingsStore            │
├───────────────────────────────────────────────────────────────────┤
│ CORE LOGIC (pure C++, no Arduino, host-testable)                  │
│  metar_parser · alert_arbiter · units · beacon_codec ·            │
│  rssi_range · attitude_math · duty_cycle · drop_tank_model        │
├───────────────────────────────────────────────────────────────────┤
│ BOARD HAL  (IBoard + capabilities)                                │
│  BoardUltra (LilyGoUltra)  ·  BoardS3 (LilyGoLib T-Watch S3)      │
├───────────────────────────────────────────────────────────────────┤
│ VENDOR: LilyGoLib · LVGL 9 · RadioLib · SensorLib · XPowersLib ·  │
│ ESP8266Audio · IRremoteESP8266 (S3) · NFC-RFAL (Ultra) · ESP-IDF  │
└───────────────────────────────────────────────────────────────────┘
```

**Dependency rule:** each layer may depend only on layers below it. Core logic depends on nothing but the C++ standard library. Pages talk to services only through the EventBus, snapshots and service command APIs. Pages never call vendor libraries.

## 4. Board HAL
```cpp
enum class Cap : uint32_t {
  Lora = 1<<0, Haptics = 1<<1, Speaker = 1<<2, Mic = 1<<3, IrTx = 1<<4,
  Nfc = 1<<5, Gnss = 1<<6, SdCard = 1<<7, ImuStepCounter = 1<<8,
  ImuWakeInterrupt = 1<<9, Amoled = 1<<10
};
enum class Rail { Display, Lora, AudioAmp, Gnss, Nfc, Sensor };

struct IBoard {
  virtual bool begin() = 0;                 // never aborts; records failures
  virtual uint32_t caps() const = 0;
  virtual DisplayInfo display() const = 0;  // w, h, shape, dpi class
  virtual void setBrightness(uint8_t pct) = 0;
  virtual void setRail(Rail, bool on) = 0;  // called only by PowerService
  virtual PmuReading readPmu() = 0;         // batt %, mA (signed), die °C, charging
  virtual bool readRtc(tm&) = 0;  virtual bool writeRtc(const tm&) = 0;
  virtual ImuSample readImu() = 0;          // accel (g), gyro (dps) if available
  virtual uint32_t stepCount() = 0;
  virtual void haptic(uint8_t effectId) = 0;
  virtual void configureWake(WakeSources) = 0;
  // accessors for Radio/Audio/Mic/Nfc/Ir/Gnss handles, each nullable
};
IBoard& board();   // selected at compile time by ARDUINO_T_WATCH_S3_ULTRA / ARDUINO_T_WATCH_S3
```
- `BoardUltra` wraps `LilyGoUltra` (`instance`). `BoardS3` wraps the T-Watch S3 class.
- **The `#ifdef` board selection lives only in `src/board/`.**
- I2C is shared by touch, IMU, PMU, RTC, haptics and the expander. Every HAL call that touches I2C shall take `i2cMutex` (a FreeRTOS mutex) because services on core 0 and touch on core 1 both use the bus.

## 5. Runtime view

### 5.1 Tasks
| Task | Core | Prio | Period / trigger | Responsibility |
| :--- | :---: | :---: | :--- | :--- |
| `ui` (Arduino `loop`) | 1 | 1 | ~5 ms loop | `lv_timer_handler()`, drain UI event queue, apply snapshots |
| `attitude` | 1 | 3 | 100 Hz | Read IMU, complementary filter → pitch/roll, peak-g, publish `AttitudeSnapshot` |
| `svc` | 0 | 2 | 50 ms tick + queue | Cooperative tick for Power, Time, Alert, Chrono, Recorder, Haptic |
| `net` | 0 | 1 | on demand | Wi-Fi up → NTP → METAR → Wi-Fi down |
| `radio` | 0 | 4 | DIO1 IRQ + timers | RadioLib RX/TX, beacons, duty-cycle accounting |
| `audio` | 0 | 5 | while playing | WAV decode → I2S. Holds the AudioAmp rail lease |
| `mic` | 0 | 4 | while PTT held | PDM capture → RMS/FFT (→ Codec2 in S13) |

The attitude task runs on core 1 at a higher priority than `ui`. This keeps sampling deterministic, and the work is tiny (<1 ms). If IMU reads ever stall on I2C contention, move it to core 0 (ADR).

### 5.2 UI thread-safety contract
1. **Only the `ui` task calls LVGL.** No exceptions.
2. Services publish **snapshots**: small POD structs in a double-buffered, seqlock-protected slot. Pages read the latest snapshot in an `lv_timer` callback (`AttitudeSnapshot`, `PowerSnapshot`, `TimeSnapshot`, `WeatherSnapshot`, `TcasSnapshot`, `RecorderSnapshot`, `ChronoSnapshot`, `MicSnapshot`).
3. Discrete happenings go through the **EventBus** (`xQueue` per subscriber, fixed-size `Event{type, u32 a, u32 b}`): `PageChanged`, `AlertRaised/Cleared`, `WeatherUpdated`, `TargetNew`, `ChronoExpired`, `SettingsChanged`, `NvgChanged`, `EjectRequested`.
4. Pages send commands to services through thread-safe command functions (`AudioService::play(Clip)`, `NetService::requestSync()`), which post to the service's queue.

### 5.3 Key sequences
**Hourly sync (S2/S3):** `TimeService` timer → `NetService::requestSync()` → `net` task: lease Wi-Fi → connect (≤15 s) → SNTP → `board().writeRtc()` → `WeatherService::fetch()` (HTTPS GET, parse with `metar_parser`) → publish `WeatherSnapshot`, `TimeSnapshot` → Wi-Fi off → events.

**Low battery (S4):** `PowerService` sees batt <15 % and not charging → `AlertManager::raise(STALL, WARNING)` → arbiter picks the highest alert → event to UI (annunciator) → `HapticService` plays the stick shaker pattern → `AudioService` plays *Master Caution* (unless quiet) → user tap → `ack`.

**Eject (S7):** UI `EjectRequested` → `PowerService::eject()`: Recorder and Chrono flush to RTC RAM + NVS → stop radio/audio/mic/gnss tasks → rails off → `board().configureWake(Button | ImuTilt | ChronoTimer)` → `esp_deep_sleep_start()`.

## 6. Module catalogue
| Module | Slice | Responsibilities | Publishes / API |
| :--- | :---: | :--- | :--- |
| `PageManager` | S0 | Page ring, swipe/button nav, lifecycle (`onCreate/onEnter/onExit/onTick`), page indicator, click haptic hook | `PageChanged` |
| `Theme` | S0/S7 | Palette tokens (sky, ground, hud-green, warning, caution, advisory, bg), fonts, scale factor from `DisplayInfo`, NVG variant | `NvgChanged` |
| `SettingsStore` | S0 | Typed NVS key/value with defaults, schema version, change events | `SettingsChanged` |
| `PowerService` | S0/S7 | Rail leases (`RailLease` RAII, refcount), PMU polling 1 Hz, screen timeout, eject | `PowerSnapshot` |
| `Log` | S0 | Levelled serial logging, no secrets | — |
| `AttitudeService` | S1 | IMU sampling, complementary filter, level lock detection, peak-g | `AttitudeSnapshot` |
| `TimeService` | S2 | RTC↔system clock, TZ, sync scheduling, last-sync age | `TimeSnapshot` |
| `NetService` | S2 | Wi-Fi lease, connect with timeout, SNTP, HTTPS client, provisioning (serial cmd / captive portal) | — |
| `WeatherService` | S3 | METAR fetch + parse, staleness | `WeatherSnapshot`, `WeatherUpdated` |
| `AlertManager` | S4 | Raise/clear/ack, priority arbitration (`alert_arbiter`), haptic/audio mapping table | `AlertRaised/Cleared` |
| `HapticService` | S4 | DRV2605 effect & pattern sequencer, intensity | — |
| `AudioService` | S5 | Clip queue, WAV from LittleFS, volume, amp rail lease, chime scheduler | — |
| `RecorderService` | S6 | Steps→NM, Max G, daily rollover, 7-day ring | `RecorderSnapshot` |
| `ChronoService` | S8 | Countdown model (`drop_tank_model`), Pomodoro, survives sleep via RTC RAM + wake timer | `ChronoSnapshot`, `ChronoExpired` |
| `RadioService` | S9 | SX1262 config per region, RX IRQ, TX queue, duty-cycle budget (`duty_cycle`) | — |
| `TcasService` | S9 | Beacon schedule, target table, ageing, RSSI→range | `TcasSnapshot`, `TargetNew` |
| `MicService` | S10 | PDM capture, RMS dBFS, FFT gesture detection | `MicSnapshot` |
| `IffService` | S11 | IR send (S3) / NFC NDEF emulate-read (Ultra), guarded fire | — |
| `GnssService` | S12 | GNSS lease, NMEA parse (TinyGPSPlus), fix snapshot | `GnssSnapshot` |
| `VoiceLinkService` | S13 | Codec2 encode/decode, fragmenting over RadioService | — |

## 7. Data & protocol design

### 7.1 Persistent storage
| Store | Medium | Contents |
| :--- | :--- | :--- |
| `SettingsStore` | NVS namespace `aerodeck` | Schema version, theme/NVG, TZ, Wi-Fi SSID/PSK, METAR station, API key, LoRa region/callsign/SF/BW/power, volumes, haptic level, quiet hours, stride length, screen timeout |
| Recorder | NVS `rec` (hourly) + `RTC_DATA_ATTR` (on sleep) | Today's steps/maxG/time + 7-day ring |
| Chrono | `RTC_DATA_ATTR` | Target end epoch, mode, state |
| Assets | FAT partition `ffat` (mounted and USB-exposed by LilyGoLib, ADR-009) `/audio/*.wav` (16-bit PCM, mono, 16 kHz) | chime, master_caution, pull_up, bingo_fuel, minimums, traffic |

The partition table is project-owned (`partitions/aerodeck_16MB.csv`, ADR-009): nvs 20 KB, otadata, app0/app1 4 MB each, ffat ≈7.9 MB, coredump 64 KB. It must not point into `libdeps/`.

### 7.2 AeroDeck LoRa beacon v1 (binary, little-endian, ≤24 B)
| Offset | Size | Field |
| :---: | :---: | :--- |
| 0 | 2 | Magic `0xAD 0x01` (protocol v1) |
| 2 | 1 | Type (`0x01` beacon, `0x02` voice-fragment, `0x03` ack) |
| 3 | 4 | Node ID (from eFuse MAC, truncated) |
| 7 | 1 | Seq |
| 8 | 8 | Callsign (ASCII, zero-padded) |
| 16 | 1 | Battery % |
| 17 | 1 | Flags (bit0 has-position) |
| 18 | 6 | Optional lat/lon (int24 each, `deg × 2²³/180`, ≈2.4 m resolution) |

Default radio profile: EU868 869.525 MHz, SF9, BW125, CR4/5, 14 dBm. US915 profile selectable. `duty_cycle` enforces the budget before every TX.

### 7.3 RSSI → range ring
`ring = clamp((rssi_ref − rssi) / step, 0, N−1)` with `rssi_ref = −40 dBm`, `step = 15 dB`, `N = 4`. Smooth with an EMA (α=0.3). The bearing is a stable hash of the node ID unless a GNSS position exists.

## 8. UI architecture
- **Resolution independence:** `Theme::scale = min(w,h)/240.0`. All sizes go through `S(px)`. Layout uses LVGL flex/grid and `lv_pct`. Ultra 502×410 is landscape: the PFD uses a centred square ADI with tapes in the side margins.
- **Horizon rendering:** prefer two layered `lv_obj` rectangles (sky/ground) in a clipping container, transformed with `lv_obj_set_style_transform_rotation/pivot` and translated for pitch. Fall back to an `lv_canvas` in PSRAM if transform cost exceeds budget (measure in S1; record the result in an ADR).
- **Tapes:** custom widget = clipped container with pre-rendered tick labels, scrolled by `translate_y` (no relayout per frame).
- **Buffers:** LVGL draw buffers in PSRAM, partial refresh, DMA enabled via `setDisplayParams(true, …)`. Invalidate only changed regions.
- **Fonts:** a HUD 7-segment style font (open licence, converted with `lv_font_conv`) at 2–3 sizes plus Montserrat for text.
- **NVG:** Theme swaps every palette token to green-on-black and sends `NvgChanged`. Pages restyle through shared `lv_style_t` objects, so they need no per-widget code.

## 9. Build & configuration
- Environments: `twatch_ultra` (primary), `twatchs3` (secondary, must compile), `native` (Unity tests of `src/core/` only: `build_src_filter = +<core/>`, `test_build_src = yes`; needs a host GCC, MinGW-w64 on Windows).
- **LilyGoLib patch (ADR-008):** `extra_scripts` runs `post:tools/patch_lilygolib.py`, which idempotently rewrites the IDF-4.4 display guard after `lib_deps` are installed and before compilation. A `pre:` script would run before the library exists on a clean checkout. A separate `pre:tools/fw_version.py` injects `AD_FW_VERSION` from `git describe`.
- **Dependency pinning (constitution VII):** every `lib_deps` entry is an exact version, and Git URLs are pinned to a commit SHA.
- `src_dir = src`. App sources under `src/{board,core,services,ui,pages,app}`.
- Compile-time feature flags (`-D AD_FEATURE_TCAS=1` …) let you exclude whole slices for size or debug builds.

```
src/
  app/        main.cpp (setup/loop), app_registry.cpp (pages+services list)
  board/      IBoard.h, BoardUltra.cpp, BoardS3.cpp, i2c_lock.h
  core/       pure logic (host-tested)
  services/   *Service.{h,cpp}, EventBus, Snapshot<T>
  ui/         PageManager, Theme, widgets/
  pages/      PfdPage, WxPage, RecorderPage, ChronoPage, TcasPage, PttPage, IffPage, GpsPage, SettingsPage
(assets)      /audio/*.wav on the FAT partition: copy over USB mass storage (ADR-009)
test/         test_core_*/  (Unity, env:native)
tools/        patch_lilygolib.py, gen_fonts.sh
```

## 10. Verification strategy
| Level | Tooling | Scope |
| :--- | :--- | :--- |
| Unit | `pio test -e native` (Unity) | Everything in `src/core/` |
| Build | `pio run -e twatch_ultra -e twatchs3` | Every slice; CI-ready |
| On-device | Serial test hooks (`ad test <name>`), debug FPS/heap overlay | Per-slice acceptance scenarios |
| Power | `PowerService` profiling log + USB meter | NFR-PWR-002 measurement after S7 |
Each slice's spec-kit `tasks.md` shall include its acceptance scenarios as on-device checks.

## 11. Architecture decision records (initial)
| ADR | Decision | Rationale |
| :--- | :--- | :--- |
| ADR-001 | Primary target is the T-Watch Ultra. S3 is secondary through the HAL. | This is the hardware in hand. The concept spec's S3-only features (IR) are capability-gated. |
| ADR-002 | LVGL 9.x (not 8.3 as in the concept spec). | Already pinned in the project and supported by LilyGoLib 0.2.0. |
| ADR-003 | Stay on Arduino-ESP32 2.0.17 / IDF 4.4 for now and patch LilyGoLib. | Proven build. A migration to 3.x is deferred (SRS OI-5). |
| ADR-004 | Only the UI task touches LVGL. Snapshots + EventBus for everything else. | Avoids LVGL locking bugs and protects frame time. |
| ADR-005 | 30 FPS guaranteed, 60 FPS aspirational. | A 502×410 QSPI panel makes full-frame 60 FPS unproven (SRS OI-2). |
| ADR-006 | Default LoRa region EU868. | The user's locale (LTAC, Türkiye). US915 is selectable. |
| ADR-007 | Default METAR source is aviationweather.gov (no key). | No account needed. CheckWX is optional. |
| ADR-008 | LilyGoLib IDF-4.4 fix applied by a `post:` extra script (`docs/adr/008-…`). | `pre:` runs before `lib_deps` install on a clean checkout (verified in PlatformIO source). |
| ADR-009 | Assets on LilyGoLib's FAT `ffat` partition, not LittleFS (`docs/adr/009-…`). | LilyGoLib already mounts FAT and exposes it over USB. |

New ADRs are appended in `docs/adr/NNN-title.md` by the slice that makes the decision.
