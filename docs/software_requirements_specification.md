# AeroDeck — Software Requirements Specification (SRS)

| Field | Value |
| :--- | :--- |
| Document ID | AD-SRS-001 |
| Version | 1.0 (initial canonical baseline) |
| Date | 2026-11-10 |
| Status | Baseline for spec-kit development |
| Source | `aerodeck_s3_architecture_specification.md` (concept spec) |
| Companion docs | `software_architecture_specification.md` (AD-SAS-001), `product_roadmap.md`, `speckit_slice_prompts.md` |

---

## 1. Introduction

### 1.1 Purpose
This document is the canonical statement of **what** the AeroDeck firmware must do. It turns the concept spec into requirements you can test, each with a unique ID. Every spec-kit feature slice traces back to the requirement IDs defined here.

### 1.2 Product summary
AeroDeck is a smartwatch firmware with a glass-cockpit look. The main watch face is a Primary Flight Display (PFD), and swiping moves between Multi-Function Display (MFD) pages. Each page puts one onboard peripheral in an avionics role: IMU → artificial horizon, PMIC → engine gauges, LoRa → TCAS radar, haptics → stick shaker, speaker → GPWS voice, microphone → push-to-talk.

### 1.3 Target hardware (reconciled)
The concept spec was written for the **LilyGO T-Watch S3**. The development unit, and the primary target of this SRS, is the **LilyGO T-Watch Ultra** (`env:twatch_ultra`). Where the boards differ, the board abstraction layer (see AD-SAS-001 §4) must absorb it.

| Concern | Concept spec (T-Watch S3) | **Primary target (T-Watch Ultra)** | Requirement impact |
| :--- | :--- | :--- | :--- |
| MCU | ESP32-S3, 8 MB PSRAM | ESP32-S3, 16 MB flash, 8 MB QSPI PSRAM | None |
| Display | 240×240 ST7789V LCD (backlight PWM) | **410×502 AMOLED over QSPI** (410 wide × 502 tall, portrait; the panel is 502×410 before LilyGoLib's default rotation; **rounded corners, radius 64 px**) (brightness 0–255) | Layout is resolution-independent; "backlight" means panel brightness; true black saves power |
| Touch | FT6336U | CST9217 | Abstracted |
| IMU | BMA423 (accel, step counter) | **BHI260AP** smart sensor hub (accel/gyro, virtual sensors) | Better attitude data; step/tilt via virtual sensors |
| RTC | PCF8563 | **PCF85063A** | Abstracted |
| PMIC | AXP2101 | AXP2101 | None |
| Haptics | DRV2605L | DRV2605 | None |
| Audio out | MAX98357A I2S | MAX98357A I2S (PCM amplifier) | None |
| Mic | PDM | PDM | None |
| LoRa | SX1262 915 MHz | SX1262 (RF switch via XL9555 expander) | Frequency must be configurable by region |
| IR LED | Yes (GPIO2) | **Not present** | IFF/IR feature becomes S3-only; Ultra uses an NFC alternative (FR-IFF) |
| Extra | — | **GNSS (GPS), NFC (ST25R3916), microSD, XL9555 IO expander** | Optional enhancements (FR-GPS) |

### 1.4 Toolchain baseline (as-built)
- PlatformIO, `framework = arduino`, `platform = espressif32@6.12.0` (Arduino-ESP32 **2.0.17 / ESP-IDF 4.4**).
- LilyGoLib **0.2.0** (pinned), LVGL **9.4** (the concept spec says 8.3; **9.x is canonical**), RadioLib 7.x, SensorLib 0.3.x, XPowersLib 0.3.x, ESP8266Audio 2.0.0.
- Known issue: LilyGoLib 0.2.0 needs a local patch to its display version guard (`LilyGoDispInterface.cpp`) to build on IDF 4.4. Slice S0 must make this patch reproducible (see AD-SAS-001 §9).

### 1.5 Definitions
| Term | Meaning |
| :--- | :--- |
| PFD | Primary Flight Display — the main watch face |
| ADI | Attitude Director Indicator — the artificial horizon |
| MFD page | One swipeable full-screen module |
| EICAS | Engine-Indicating and Crew-Alerting System — the PMIC telemetry bar |
| TCAS | Traffic Collision Avoidance System — the LoRa proximity radar |
| GPWS | Ground Proximity Warning System — the voice alert system |
| PTT | Push-to-talk |
| NVG | Night-vision-goggle display mode |
| Zulu | UTC time |
| Alert levels | WARNING (red, highest), CAUTION (amber), ADVISORY (cyan/white) |

### 1.6 Requirement conventions
- IDs: `FR-<AREA>-NNN` (functional) and `NFR-<AREA>-NNN` (non-functional).
- Priority: **M** = Must (MVP), **S** = Should, **C** = Could / experimental.
- "Shall" is mandatory; "should" is recommended.

---

## 2. Overall description

### 2.1 Users
- **Primary:** an aviation-enthusiast maker who wears the watch daily and develops it.
- **Secondary:** a paired peer (another AeroDeck or LoRa/Meshtastic node) for TCAS and walkie-talkie.

### 2.2 Operating constraints
- Battery powered. Any feature that keeps the radio, Wi-Fi or audio on must have a defined duty cycle.
- The device must stay usable with no network. Wi-Fi only adds data (NTP time, METAR).
- The device must comply with local RF regulations: the LoRa frequency and power follow the configured region (default **EU868** for Türkiye/LTAC; US915 selectable).

### 2.3 Assumptions
- A LittleFS (or FAT) partition holds audio assets. The microSD card is optional storage.
- METAR comes from a public API (default `aviationweather.gov` JSON, no key needed). CheckWX is optional and needs a user API key.

---

## 3. Functional requirements

### 3.1 Platform & navigation (PLT)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| FR-PLT-001 | M | The system shall boot to the PFD watch face within 4 s of power-on or wake from deep sleep. *(Amended in S0 from 3 s; the measured hardware/vendor floor is ~3.45 s.)* |
| FR-PLT-002 | M | The system shall show MFD pages in a fixed ring. Swiping left/right shall move to the next/previous page, wrapping at the ends. |
| FR-PLT-003 | M | A short press of the physical button shall return to the PFD from any page. A press on the PFD shall go to the most recently used MFD page. |
| FR-PLT-004 | M | Each page shall get lifecycle events (enter, exit, suspend). A page shall release hardware it no longer needs on exit. |
| FR-PLT-005 | M | User settings (theme, NVG, timezone, Wi-Fi credentials, LoRa region/callsign, METAR station, volume, haptic intensity) shall persist across reboots and deep sleep. |
| FR-PLT-006 | M | A Settings page shall let the user edit every persisted setting on the watch. Wi-Fi credentials may instead be provisioned over serial or a captive portal. |
| FR-PLT-007 | S | The system shall show a page indicator (dots or a label) for 1 s after each page change. |
| FR-PLT-008 | M | The firmware shall log structured diagnostics over USB serial at a configurable level. |

### 3.2 Primary Flight Display (PFD)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| FR-PFD-001 | M | The PFD shall draw an artificial horizon (blue sky, brown ground). It shall rotate with wrist roll and shift with wrist pitch, using IMU attitude. |
| FR-PFD-002 | M | When pitch and roll are both within ±3° for ≥0.5 s, a green **LOCK** indicator shall light at the crosshair. It shall clear when either axis leaves ±5° (hysteresis). |
| FR-PFD-003 | M | A left vertical tape ("airspeed") shall show local **hours 00–23**, with the current value in a highlighted box. |
| FR-PFD-004 | M | A right vertical tape ("altitude") shall show **minutes 00–59** in the same style. |
| FR-PFD-005 | M | A top heading tape shall show **seconds** as compass degrees (1 s = 6°, 0–354°), scrolling smoothly between seconds. |
| FR-PFD-006 | M | The bottom corners shall show **ZULU (UTC)** and **LOCAL** time as HH:MM in a high-contrast HUD style. |
| FR-PFD-007 | M | An EICAS strip shall show **FUEL QTY %** (battery %), **FUEL FLOW** (battery **drain rate in %/h**, estimated from the battery-percentage trend over ≥10 min; "--" until enough history exists, a refuel indication while charging), and **EGT** (PMIC die temperature in °C). It shall refresh at ≥1 Hz. *(Amended in S1: the AXP2101 has no battery-current ADC, so mA cannot be measured.)* |
| FR-PFD-008 | S | The PFD shall show **Max G** (today's peak) and **Flight Miles** (today's NM), sourced from FR-REC. |
| FR-PFD-009 | S | The PFD shall show a charging annunciator and a low-battery CAUTION annunciator, driven by the Alert Manager. |
| FR-PFD-010 | C | If GNSS has a fix, the heading tape may optionally show true course over ground (user setting). Seconds then move to a numeric readout. |
| FR-PFD-011 | M | While the attitude reference is initialising, the ADI shall show **ATT ALIGN**. If motion sensing is unavailable it shall show a level picture with **ATT FAIL**, and every other PFD function shall keep working. *(Added in S1.)* |

### 3.3 Time & connectivity (TIM / NET)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| FR-TIM-001 | M | The system clock shall start from the hardware RTC on boot and wake. |
| FR-TIM-002 | M | With Wi-Fi credentials set, the system shall sync time by NTP at boot, then every 60 min (configurable), and write the result back to the RTC. |
| FR-TIM-003 | M | Local time shall follow a user-selected **named time zone** with its summer-time rules (built-in table of ≥40 zones covering every UTC offset in use, POSIX TZ rules; default Istanbul, UTC+3). ZULU always shows UTC. *(S2: replaces the S1 interim fixed `tzoff` offset, which is migrated to an equivalent zone.)* |
| FR-TIM-004 | S | The UI shall show the time since the last successful sync and a "TIME UNSYNC" ADVISORY when it exceeds 24 h. |
| FR-TIM-005 | C | When Wi-Fi is unavailable, the system may set the RTC from GNSS time. |
| FR-TIM-006 | M | If the RTC lost its time (clock integrity not guaranteed) or holds a year before 2025, all time readouts shall show "--" and a **CLOCK NOT SET** caution shall be raised. *(Added in S1.)* |
| FR-TIM-007 | M | The UTC clock shall remain settable via the diagnostic serial console (`ad time set`) as a **fallback when no network is available**, and kept in the RTC across reboots. *(Added in S1; role changed in S2.)* |
| FR-NET-001 | M | Wi-Fi shall be on only during a sync/fetch transaction and turned off afterwards (no persistent association). |
| FR-NET-002 | M | A network transaction shall time out after ≤15 s and shall never block UI rendering. |
| FR-NET-003 | M | The phone setup hotspot (Wi-Fi provisioning) shall start only on a deliberate action on the watch, be protected by a one-time access code shown only on the watch, close after ≤5 min or when credentials are saved or setup is cancelled, and never overlap a sync transaction. *(Added in S2.)* |

### 3.4 METAR weather page (WX)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| FR-WX-001 | M | The system shall fetch the METAR for a configured ICAO station (default **LTAC**) at each hourly sync and on demand (a refresh control). *(S3: the scheduled fetch runs in the same Wi-Fi session as the time sync.)* |
| FR-WX-002 | M | The WX page shall show: station, observation time (Z), wind direction (°) and speed (kt, gusts if present), visibility, QNH (hPa), temperature/dewpoint, and flight category (VFR/MVFR/IFR/LIFR, colour-coded). |
| FR-WX-003 | M | The WX page shall show the raw METAR text. |
| FR-WX-004 | M | Stale data (>90 min old) shall be marked STALE, and a fetch failure shall show the last good data with an error annunciator. |
| FR-WX-005 | S | A wind arrow/compass graphic shall show wind direction relative to north. |
| FR-WX-006 | M | The METAR station shall be selectable on the watch and over the diagnostic console, validated as a 4-letter ICAO code, and persisted (default LTAC); a change triggers a fetch. *(Added in S3.)* |

### 3.5 Alerting: haptics & audio (ALR / HAP / AUD)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| FR-ALR-001 | M | A central Alert Manager shall accept raise/clear requests with a level (WARNING/CAUTION/ADVISORY), an ID and a message, and show the highest active alert as a master annunciator. |
| FR-ALR-002 | M | Each alert type shall map to a configurable haptic pattern and an optional voice callout. |
| FR-ALR-003 | M | The user shall be able to acknowledge an alert (tap the annunciator), which stops repeating haptic/audio output. |
| FR-ALR-004 | M | A quiet mode setting shall suppress audio while keeping haptics for WARNING-level alerts. *(S4: quiet mode also silences CAUTION/ADVISORY vibration; UI clicks are unaffected.)* |
| FR-HAP-001 | M | UI page changes and toggle controls shall play a crisp click waveform (DRV2605 library effect, default *Sharp Click*). |
| FR-HAP-002 | M | Battery dropping below **15 %** (not charging) shall raise a WARNING with a pulsing heavy **STICK SHAKER** pattern and a "STALL" annunciator. *(S4: repeats every 2 s until acknowledged; ≤5 % escalates to PULL UP (haptic in S4, voice in S5); clears at ≥17 % / ≥7 % (2 % hysteresis) or on charging; replaces the S1 LOW FUEL caution.)* |
| FR-HAP-003 | S | Haptic intensity shall be user-selectable (off / low / high). *(S4: default low; WARNING patterns always play at full strength, even when off.)* |
| FR-AUD-001 | M | The system shall play WAV assets from the on-board filesystem through the I2S amplifier. |
| FR-AUD-002 | M | A cabin chime shall play at the top of every hour, unless quiet mode is on or night hours are set (configurable, default 22:00–07:00). |
| FR-AUD-003 | M | Voice callouts shall exist for at least: *Master Caution*, *Pull Up* (critical battery ≤5 %), *Bingo Fuel* (timer end), *Minimums* (generic alarm). |
| FR-AUD-004 | M | The amplifier shall be powered only while audio is playing (see NFR-PWR-003). |
| FR-AUD-005 | S | Volume shall be user-adjustable in at least 4 steps. |

> **Asset note:** Voice callouts must be original recordings or TTS clips. Commercial avionics or film audio must not be copied.

### 3.6 G-Force & flight recorder (REC)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| FR-REC-001 | M | The system shall count daily steps and convert them to **Flight Miles (NM)** using a user-set stride length (default 0.75 m; 1 NM = 1852 m). |
| FR-REC-002 | M | The system shall track the peak acceleration magnitude in **g** (gravity-included magnitude, sampled at ≥50 Hz), and keep the daily **Max G** and the time it occurred. |
| FR-REC-003 | M | Daily counters shall reset at local midnight. The previous 7 days shall be kept. |
| FR-REC-004 | M | A recorder page shall show today's NM, steps, Max G, and a 7-day bar chart. |
| FR-REC-005 | M | Recorder data shall survive deep sleep and reboot (written to RTC memory before sleep and to flash at least hourly). |

### 3.7 Chronograph — "Drop Tank" timer (CHR)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| FR-CHR-001 | M | The user shall set a countdown (presets 5/15/25/50 min, plus custom) and start, pause, resume and reset it. |
| FR-CHR-002 | M | The remaining time shall be shown as an external drop tank fuel gauge that depletes as time runs down, plus an MM:SS readout. |
| FR-CHR-003 | M | At zero, the system shall raise a "BINGO FUEL" CAUTION (haptic plus *Bingo Fuel* callout). |
| FR-CHR-004 | M | A running timer shall keep counting and still fire its alert when the user changes pages or the device enters light/deep sleep (RTC/timer wake). |
| FR-CHR-005 | S | A Pomodoro mode shall alternate work and break intervals automatically. |

### 3.8 Display modes & power UX (NVG / EJT)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| FR-NVG-001 | M | An NVG toggle shall switch the whole UI to a black background with monochrome phosphor green (`#00FF66`) and set display brightness to 10 % of maximum. |
| FR-NVG-002 | M | NVG state shall persist and apply at once to every page, including live widgets. |
| FR-NVG-003 | S | Alert colours shall stay recognisable in NVG mode (WARNING shown with blink/inverse instead of red). |
| FR-EJT-001 | M | A yellow/black striped "PULL TO EJECT" control at the top of the screen, triggered by double-tap then swipe-down, shall start emergency deep sleep. |
| FR-EJT-002 | M | Eject shall: save recorder/timer state, cut power to the display, LoRa, audio amplifier and GNSS via the PMIC, configure wake sources, and enter ESP32-S3 deep sleep within 2 s. |
| FR-EJT-003 | M | Wake sources shall include the physical button and an IMU wrist-tilt / motion interrupt (where the board supports it). On wake, the system shall restore state and show the PFD (FR-PLT-001). |
| FR-EJT-004 | S | Auto screen-off after a configurable idle time (default 15 s), with wrist-raise to wake the screen. |

### 3.9 TCAS / tactical radar (TCS)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| FR-TCS-001 | M | The system shall send an AeroDeck beacon packet (protocol version, callsign, sequence, battery %, optional GNSS position) at a configurable interval (default 30 s) while TCAS is enabled. |
| FR-TCS-002 | M | The system shall receive beacons in the background and keep a target table (ID, callsign, last RSSI, last SNR, last seen, optional position). Targets age out after a configurable time (default 5 min). |
| FR-TCS-003 | M | The TCAS page shall show a sweeping PPI radar. Each target is drawn as a "bogey" at a range ring derived from RSSI (stronger = closer). Without GNSS positions, bearing is cosmetic and stable per target. |
| FR-TCS-004 | M | Tapping a target shall show its callsign, RSSI, SNR, age and battery. |
| FR-TCS-005 | S | When both ends have GNSS, targets shall be plotted at true bearing and distance. |
| FR-TCS-006 | S | A new target entering the inner ring shall raise a "TRAFFIC" ADVISORY. |
| FR-TCS-007 | C | The system may passively decode Meshtastic node-info/position packets on the same channel preset and show them as targets (read-only). |
| FR-TCS-008 | M | The LoRa region, frequency, TX power, spreading factor and bandwidth shall be configurable and limited to the legal values for the selected region. |

### 3.10 Push-to-talk / microphone (PTT)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| FR-PTT-001 | M | The PTT page shall have a hold-to-talk button that turns the microphone on only while held. |
| FR-PTT-002 | M | **dB meter mode:** while the button is held, the page shall show the ambient sound level (dBFS, ≥10 Hz update) as a gauge, with a peak-hold marker. |
| FR-PTT-003 | S | **Command mode:** recognise a small fixed vocabulary or acoustic gestures (minimum: whistle and double-clap via FFT; keywords "Night Mode", "Radar", "Timer" where on-device keyword spotting is feasible), and run the mapped action. |
| FR-PTT-004 | C | **Walkie-talkie mode:** record up to 3 s of speech, encode with Codec2 at ≤1200 bps, send it over LoRa to the paired peer, and play received clips through the speaker. |
| FR-PTT-005 | M | The microphone shall never record or transmit unless the PTT button is held. No audio shall be stored persistently by default. |

### 3.11 IFF / countermeasures (IFF)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| FR-IFF-001 | M | The IFF page shall show a red guarded "FIRE / CHAFF" cover. The trigger shall be enabled only after the cover is flipped open, and the cover shall close automatically after 10 s. |
| FR-IFF-002 | S | **T-Watch S3 build:** firing shall send a user-selected pre-programmed IR code (e.g. TV power, A/C toggle) from the IR LED. |
| FR-IFF-003 | S | **T-Watch Ultra build (no IR LED):** firing shall perform an NFC action instead, e.g. emit an NDEF tag (URL/text) for a phone to read, or read and identify a tapped NFC tag ("IFF interrogate"). |
| FR-IFF-004 | M | The page shall show clearly when the selected action is not supported on the current board. |

### 3.12 GNSS enhancements (GPS) — Ultra only, optional
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| FR-GPS-001 | C | A GNSS page shall show fix status, satellites, position, ground speed (kt) and course. |
| FR-GPS-002 | C | GNSS shall be powered only while a consuming feature is active (GPS page, TCAS with position, time sync fallback). |

---

## 4. Non-functional requirements

### 4.1 Performance (PERF)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| NFR-PERF-001 | M | The PFD horizon shall render at a sustained **≥30 FPS** on the Ultra (410×502). The goal is 60 FPS where measured headroom allows. Frame time shall be measurable via a debug overlay. |
| NFR-PERF-002 | M | IMU-to-display latency for the horizon shall be ≤50 ms. |
| NFR-PERF-003 | M | No radio, network, filesystem or audio operation shall stall the UI task for more than 1 frame (33 ms). |
| NFR-PERF-004 | M | Page transitions shall finish in ≤300 ms. |
| NFR-PERF-005 | M | Free internal heap shall stay ≥40 KB in every state. LVGL draw buffers and large assets shall live in PSRAM. |

### 4.2 Power (PWR)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| NFR-PWR-001 | M | Each power-hungry peripheral (LoRa, audio amp, GNSS, NFC, Wi-Fi, mic) shall be powered only while a consumer holds a reference to it (reference-counted power gating). |
| NFR-PWR-002 | M | With TCAS, audio and GNSS off and screen-off timeout on, typical daily use shall reach **≥48 h** of battery life. This is a target to measure, not a guarantee. |
| NFR-PWR-003 | M | The audio amplifier rail shall be off whenever no audio is playing. |
| NFR-PWR-004 | S | Firmware shall expose a power-profiling mode that logs rail states and battery current once per second. |

### 4.3 Reliability (REL)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| NFR-REL-001 | M | The firmware shall boot to a usable PFD even if any optional peripheral (LoRa, NFC, GNSS, audio, SD) fails to start. The failure shall show as a CAUTION, not a crash. |
| NFR-REL-002 | M | A task watchdog shall cover the UI and service tasks. |
| NFR-REL-003 | M | Persisted data shall be written atomically (NVS, or write-temp-then-rename) so power loss cannot corrupt settings. |

### 4.4 Usability (USE)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| NFR-USE-001 | M | Touch targets shall be ≥48 px on the Ultra (scaled for other boards). |
| NFR-USE-002 | M | Critical readouts (time, battery) shall be readable in daylight (minimum text height 24 px on the Ultra). |
| NFR-USE-003 | M | Every destructive or transmitting action (eject, IFF fire, PTT transmit) shall need a deliberate gesture (guard, hold or double-action). |

### 4.5 Portability & maintainability (MNT)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| NFR-MNT-001 | M | Application code shall use the board abstraction layer only. LilyGoLib/driver types shall not appear in pages or services. |
| NFR-MNT-002 | M | UI layout shall be resolution-independent: no hard-coded 240×240 or 410×502 coordinates outside a layout/theme module. Content shall stay inside the visible area of screens with rounded corners (Ultra: radius 64 px); corner-anchored elements shall be inset accordingly. *(Extended in S1.)* |
| NFR-MNT-003 | M | `pio run -e twatch_ultra` shall build from a clean checkout with no manual steps (including the LilyGoLib patch). |
| NFR-MNT-004 | S | Pure logic (unit conversion, METAR parsing, alert arbitration, RSSI→range mapping, beacon codec) shall have host-side unit tests (`pio test -e native`). |
| NFR-MNT-005 | S | The T-Watch S3 build (`env:twatchs3`) shall compile, with board-specific features disabled cleanly. |

### 4.6 Security & privacy (SEC)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| NFR-SEC-001 | M | Wi-Fi credentials and API keys shall be stored in NVS and never logged. |
| NFR-SEC-002 | M | HTTPS shall be used for METAR APIs that support it. |
| NFR-SEC-003 | S | LoRa beacons shall carry no personal data beyond the user-chosen callsign and (opt-in) position. |

### 4.7 Regulatory (REG)
| ID | Pri | Requirement |
| :--- | :--- | :--- |
| NFR-REG-001 | M | LoRa transmissions shall respect the regional duty-cycle and power limits (EU868: ≤1 % duty cycle on the default sub-band). The firmware shall enforce the duty-cycle budget. |

---

## 5. External interfaces
- **USB-CDC serial:** logs, provisioning commands, debug.
- **Wi-Fi (2.4 GHz):** NTP (`pool.ntp.org` default), METAR HTTPS API.
- **LoRa:** AeroDeck beacon/voice protocol (AD-SAS-001 §7). Optional passive Meshtastic decode.
- **NFC (Ultra):** NDEF tag emulation/read.
- **IR (S3):** consumer IR protocols via IRremoteESP8266.
- **Filesystem:** LittleFS/FAT partition with `/audio/*.wav` assets. Optional microSD.

---

## 6. Traceability: requirements → slices
| Slice | Title | Requirements |
| :--- | :--- | :--- |
| S0 | Platform foundation | FR-PLT-001…008, NFR-REL-001…003, NFR-MNT-001…003, NFR-PERF-003/005, NFR-PWR-001 |
| S1 | PFD watch face | FR-PFD-001…009, FR-PFD-011, FR-TIM-003 (interim), FR-TIM-006, FR-TIM-007, NFR-PERF-001/002, NFR-USE-002 |
| S2 | Time sync & connectivity | FR-TIM-001…004, FR-TIM-003 (named zones), FR-TIM-006, FR-TIM-007 (fallback), FR-NET-001…003, NFR-SEC-001 |
| S3 | METAR weather page | FR-WX-001…006, NFR-SEC-002, NFR-MNT-004 |
| S4 | Alert Manager & haptics | FR-ALR-001…004, FR-HAP-001…003 |
| S5 | GPWS audio | FR-AUD-001…005, NFR-PWR-003 |
| S6 | G-force & flight recorder | FR-REC-001…005, FR-PFD-008 |
| S7 | NVG, eject & power UX | FR-NVG-001…003, FR-EJT-001…004, NFR-PWR-002/004 |
| S8 | Drop tank chronograph | FR-CHR-001…005 |
| S9 | TCAS LoRa radar | FR-TCS-001…008, NFR-REG-001, NFR-SEC-003 |
| S10 | PTT: dB meter & acoustic commands | FR-PTT-001…003, FR-PTT-005 |
| S11 | IFF countermeasures | FR-IFF-001…004 |
| S12 | GNSS enhancements | FR-GPS-001/002, FR-PFD-010, FR-TIM-005, FR-TCS-005 |
| S13 | LoRa walkie-talkie (experimental) | FR-PTT-004 |

---

## 7. Open issues
| # | Issue | Proposed resolution |
| :--- | :--- | :--- |
| OI-1 | Can the BHI260AP wake the ESP32-S3 from deep sleep directly (interrupt pin routing on Ultra)? | Verify in S7. Fall back to button-only wake plus timer wake for the chronograph. |
| OI-2 | Is 60 FPS full-horizon redraw feasible at 410×502 over QSPI? | Measure in S1. NFR-PERF-001 guarantees 30 FPS. S0 measured full-screen page transitions at 15–28 FPS with LilyGoLib's 1/6-screen DMA buffers; S1 must tune rendering (buffer size, partial invalidation, lighter styles). |
| OI-3 | On-device keyword spotting memory/CPU budget. | S10 starts with FFT acoustic gestures. Keyword spotting is a stretch goal. |
| OI-4 | Codec2 on Arduino-ESP32 2.x build integration. | S13 is experimental and gated by a spike. |
| OI-5 | Migrate to Arduino-ESP32 3.x (IDF 5) to drop the LilyGoLib patch? | Re-evaluate after S5. A migration would be its own slice. |
| OI-6 | SX1262 band variant of the physical unit (868 vs 915 MHz hardware). | Confirm from the unit label before S9. Default region EU868. |
