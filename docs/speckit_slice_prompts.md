# AeroDeck — spec-kit Prompts per Slice

Copy-paste prompts for [GitHub spec-kit](https://github.com/github/spec-kit). For each slice, run them in this order:

1. `/speckit.specify` — **what & why** (no tech details). Creates the feature branch + `spec.md`.
2. `/speckit.clarify` — optional, resolves `[NEEDS CLARIFICATION]` markers.
3. `/speckit.plan` — **how** (tech stack & architecture). Creates `plan.md`, `research.md`, `data-model.md`, `contracts/`, `quickstart.md`.
4. `/speckit.tasks` → `/speckit.analyze` (optional consistency check) → `/speckit.implement`.

Canonical references the agent must read (paths relative to repo root):
- `docs/software_requirements_specification.md` (AD-SRS-001): requirement IDs
- `docs/software_architecture_specification.md` (AD-SAS-001): architecture, modules, ADRs
- `docs/product_roadmap.md`: slice order and exit criteria

---

## Step 0 — Constitution (run once, before S0)

```text
/speckit.constitution Create the AeroDeck firmware constitution. AeroDeck is glass-cockpit smartwatch firmware for the LilyGO T-Watch Ultra (primary) and T-Watch S3 (secondary). The canonical documents are docs/software_requirements_specification.md (AD-SRS-001) and docs/software_architecture_specification.md (AD-SAS-001). Every spec must trace to SRS requirement IDs, and every plan must comply with the SAS or add an ADR under docs/adr/.

Principles:
I. Every slice ships: each feature ends as flashable, wearable firmware. The watch must always boot to a usable PFD watch face, even if optional peripherals fail (failures become CAUTION alerts, never crashes or boot loops).
II. Layered architecture is law: UI → Services → Core logic → Board HAL → vendor libraries. Only the UI task calls LVGL. Services communicate with the UI through snapshots and the EventBus. Pages and services never use LilyGoLib or driver types directly. Board #ifdefs live only in src/board/.
III. Battery is a feature: power-hungry peripherals (LoRa, audio amp, GNSS, NFC, Wi-Fi, microphone) are powered only while a consumer holds a RailLease. Wi-Fi is never left associated.
IV. Frame time is sacred: no blocking I/O on the UI task. The PFD horizon holds ≥30 FPS. Heap ≥40 KB free. Large buffers live in PSRAM.
V. Test what can be tested off-device: pure logic lives in src/core/ with no Arduino dependency and Unity tests under env:native. Every slice adds on-device acceptance checks to tasks.md.
VI. Deliberate actions for anything destructive or transmitting (eject, IR/NFC fire, PTT transmit). RF use is region-compliant with an enforced duty-cycle budget. The microphone works only while PTT is held.
VII. Privacy & secrets: credentials and API keys only in NVS, never logged. No personal data in radio packets beyond the user-chosen callsign and opt-in position.
VIII. Reproducible builds: `pio run -e twatch_ultra` and `pio run -e twatchs3` succeed from a clean checkout with no manual steps. Dependency versions are pinned.
IX. Simplicity: no speculative abstractions. Experimental features (Codec2 voice, keyword spotting) stay behind compile-time feature flags.

Governance: the constitution supersedes plan preferences. Amendments need a version bump and a note in the roadmap. Each /speckit.plan must include a Constitution Check section.
```

---

## S0 — Platform foundation

**/speckit.specify**
```text
/speckit.specify Platform foundation for AeroDeck, the glass-cockpit smartwatch firmware (see docs/software_requirements_specification.md, slice S0). As the wearer, I want the watch to boot quickly into a home screen, move between full-screen "MFD pages" by swiping left and right in a wrapping ring, and return home with a single press of the physical button. That way every future cockpit module plugs into the same navigation. As the developer, I want settings that survive reboots and sleep, a Settings page to view and edit them, a debug overlay showing frame rate and free memory, and diagnostics over USB serial. The watch must still boot to the home screen when an optional peripheral is missing or broken, and show a caution message instead of crashing. Also in scope: making the firmware build reproducibly from a clean checkout with no manual steps. Covers FR-PLT-001..008, NFR-REL-001..003, NFR-MNT-001..003, NFR-PERF-003/005, NFR-PWR-001. Acceptance: cold boot to home in under 3 s; swiping cycles through home → placeholder page A → placeholder page B → Settings → home; button returns home; a changed setting is still there after a reboot; disconnecting a peripheral at boot yields a caution, not a crash. Out of scope: the real PFD face, alerts, audio and radio features.
```

**/speckit.plan**
```text
/speckit.plan Follow docs/software_architecture_specification.md (AD-SAS-001) §3–§6 and §9. Stack: PlatformIO, framework arduino, platform espressif32@6.12.0 (Arduino-ESP32 2.0.17 / ESP-IDF 4.4), env twatch_ultra primary and twatchs3 must compile, add env:native for Unity tests of src/core/. Libraries as pinned in platformio.ini: LilyGoLib 0.2.0 exactly, LVGL 9.4, SensorLib, XPowersLib. Build reproducibility: LilyGoLib 0.2.0 fails on IDF 4.4 at src/LilyGoDispInterface.cpp (guard `#if (ESP_ARDUINO_VERSION <= ESP_ARDUINO_VERSION_VAL(4,0,0))` selects IDF5-only rgb_ele_order/data_endian). Fix it reproducibly with an idempotent `extra_scripts = pre:tools/patch_lilygolib.py` that replaces the guard with `#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)` and sets vendor_config=nullptr. Verify by deleting .pio and rebuilding, and record ADR-008 (falling back to vendoring under lib/ if the pre-script runs too late). Create the src/ layout from SAS §9 (app, board, core, services, ui, pages). Implement IBoard with capability flags + BoardUltra (wraps LilyGoUltra `instance`) + BoardS3, an i2c mutex, EventBus (per-subscriber FreeRTOS queues, fixed-size events), Snapshot<T> (seqlock double buffer), SettingsStore (Preferences/NVS, namespace "aerodeck", schema version, typed defaults), PowerService with RAII RailLease refcounting and 1 Hz PMU polling, Log with levels, PageManager (page ring, swipe via LVGL gesture events, button → home, lifecycle onCreate/onEnter/onExit/onTick, page indicator), Theme with scale factor S(px)=px*min(w,h)/240 and palette tokens, SettingsPage, and a debug FPS/heap overlay. Task layout per SAS §5.1: UI in Arduino loop on core 1, `svc` cooperative tick task on core 0. Move the partition table to project-owned partitions/aerodeck_16MB.csv with a LittleFS partition ≥2 MB. Replace the current hello-world src/main.cpp. Unit-test EventBus-independent core helpers on native.
```

---

## S1 — PFD watch face

**/speckit.specify**
```text
/speckit.specify Primary Flight Display watch face for AeroDeck (slice S1, FR-PFD-001..009, NFR-PERF-001/002, NFR-USE-002). As the wearer, I want my home screen to look like an aircraft PFD. A blue-sky/brown-ground artificial horizon rotates and shifts as I tilt my wrist (roll and pitch). A green LOCK indicator lights at the crosshair when I hold my wrist level (within ±3° for half a second; it clears beyond ±5°). The time is read like flight instruments: a left vertical "airspeed" tape shows hours 00–23, a right vertical "altitude" tape shows minutes 00–59, and a top "heading" compass tape scrolls smoothly with the seconds (one second = 6°). The bottom corners show ZULU (UTC) and LOCAL time in a high-contrast green HUD font. An EICAS strip maps the battery to engine gauges: FUEL QTY (battery %), FUEL FLOW (battery current in mA, with charging shown distinctly) and EGT (power chip temperature in °C). It also shows a charging annunciator. It must feel fluid: at least 30 frames per second and no visible lag between wrist motion and horizon. Max G and Flight Miles have reserved slots filled with "--" until slice S6. Acceptance: tilting the wrist moves the horizon correctly in both axes; LOCK behaves with hysteresis; tapes show the correct time and roll over correctly at 23:59:59; EICAS values match the power chip's readings within 1 s; the FPS overlay shows ≥30.
```

**/speckit.plan**
```text
/speckit.plan Follow AD-SAS-001 §5 and §8. Add AttitudeService: a 100 Hz FreeRTOS task on core 1 (priority above UI) reading the IMU through IBoard::readImu() (BHI260AP accel+gyro on Ultra via SensorLib SensorBHI260AP; accel-only fallback on S3). Pure complementary filter in src/core/attitude_math (unit-tested on native: pitch/roll from gravity vector, filter convergence, lock hysteresis). Publishes AttitudeSnapshot{pitch, roll, level_locked, g_mag}. Build reusable LVGL 9 widgets in src/ui/widgets/: HorizonWidget (sky/ground objects in a clipping container using style transform rotation + translate_y for pitch; measure frame time and fall back to a PSRAM lv_canvas if needed, recording ADR-009 with numbers), TapeWidget (vertical/horizontal, pre-rendered tick labels, scrolled by translate, highlighted current-value box), Annunciator. PfdPage composes them. Portrait 410×502 layout (LilyGoLib rotation 0): heading tape on top, hour/minute tapes beside a centred square ADI, EICAS below, Zulu/Local row at the bottom; all sizes via Theme S(px). Convert an open-licence 7-segment font with lv_font_conv at 2–3 sizes. Time comes from the system clock (TimeSnapshot stub from RTC until S2). EICAS from PowerService PowerSnapshot (battery %, signed current mA, die temp, charging). Use partial invalidation only; enable display DMA via setDisplayParams(true, ...). Keep LVGL calls on the UI task only. Acceptance tasks include an on-device FPS measurement logged to serial.
```

---

## S2 — Time sync & connectivity

**/speckit.specify**
```text
/speckit.specify Accurate time and Wi-Fi connectivity for AeroDeck (slice S2, FR-TIM-001..005, FR-NET-001/002, NFR-SEC-001). As the wearer, I want the watch to always show correct UTC and local time without thinking about it. On boot and wake it uses its hardware clock. When it knows my Wi-Fi network, it syncs with internet time at boot and then every hour (adjustable), saves the result to the hardware clock, and turns Wi-Fi off again straight away to save battery. I want to set my Wi-Fi network and my time zone (default Türkiye, UTC+3) from the watch or a computer. I want to see how long ago the last sync was, and get an advisory if the time hasn't synced for 24 hours. A network attempt must never freeze the screen and must give up within 15 seconds. My Wi-Fi password must never appear in logs. Acceptance: with valid credentials the time is corrected after boot and the hardware clock keeps it across power cycles with Wi-Fi off; Wi-Fi is off between syncs; a wrong password or missing network times out gracefully with the UI staying smooth; Zulu and Local on the PFD differ by the configured offset.
```

**/speckit.plan**
```text
/speckit.plan Follow AD-SAS-001 §5.3 (hourly sync sequence) and §6. Implement TimeService (svc tick on core 0): on boot set the system time from IBoard::readRtc (PCF85063A on Ultra, PCF8563 on S3 via SensorLib); TZ via setenv("TZ")/tzset from a POSIX string in SettingsStore (default "<+03>-3"); schedule sync every N minutes (default 60); publish TimeSnapshot{utc, local, last_sync_age, synced}. Implement NetService as an on-demand `net` FreeRTOS task on core 0: acquire Wi-Fi, WiFi.begin with a 15 s timeout, SNTP via configTime/sntp with pool.ntp.org, writeRtc, then WiFi.disconnect(true)+WiFi.mode(WIFI_OFF). Provide a provisioning path: a USB serial command set (`ad wifi set <ssid> <psk>`, `ad tz set <posix>`, `ad sync`) and Settings page entries (on-screen keyboard for SSID/PSK). Store credentials in NVS. Redact them in Log. Raise a "TIME UNSYNC" advisory event when age >24 h (consumed by AlertManager in S4; until then show it on the Settings page). Add an HTTPS client helper (WiFiClientSecure with a CA bundle) for S3. Unit-test TZ/age formatting helpers on native.
```

---

## S3 — METAR weather page

**/speckit.specify**
```text
/speckit.specify METAR aviation weather page for AeroDeck (slice S3, FR-WX-001..005, NFR-SEC-002, NFR-MNT-004). As the wearer, I want a weather MFD page that shows the latest aviation weather report (METAR) for my home airport (default LTAC, changeable in settings). It shows: station, observation time in Zulu, wind direction and speed in knots with gusts, visibility, QNH pressure in hPa, temperature/dewpoint, a colour-coded flight category (VFR green, MVFR blue, IFR red, LIFR magenta), a wind arrow on a compass, and the raw METAR text. The weather refreshes with the hourly time sync, and a refresh button fetches it on demand. Data older than 90 minutes is marked STALE. A failed fetch keeps the last good report and shows an error indicator. Acceptance: the decoded fields match the raw METAR for a set of real reports (including variable wind, gusts, CAVOK, and missing fields); the page works offline with cached data; the refresh button triggers a fetch without freezing the UI.
```

**/speckit.plan**
```text
/speckit.plan Follow AD-SAS-001 §6–§7. WeatherService runs inside the S2 `net` task sequence after SNTP: HTTPS GET https://aviationweather.gov/api/data/metar?ids=<ICAO>&format=json (no key; optional CheckWX provider with an API key from NVS behind a provider interface). Parse JSON with ArduinoJson (pin the version) for the observation time and raw text. Decode the raw METAR in a pure src/core/metar_parser (wind dddssKT/ddd ssGggKT/VRB/variable sector, visibility m/SM/CAVOK, cloud layers for the ceiling, QNH Qnnnn or Annnn converted to hPa, temp/dew incl. M-negatives) and compute the flight category from ceiling/visibility. Unit-test it on native with ≥15 real METAR strings. Cache the last good WeatherSnapshot in NVS so it shows after reboot. WxPage: tiles laid out with Theme S(px), wind compass widget, category colour from theme tokens (NVG-aware later), raw text in a scrolling label, a refresh button that posts NetService::requestSync(WeatherOnly). Staleness is computed from TimeSnapshot.
```

---

## S4 — Alert Manager & haptics ("Stick Shaker & Flaps")

**/speckit.specify**
```text
/speckit.specify Cockpit alerting and haptics for AeroDeck (slice S4, FR-ALR-001..004, FR-HAP-001..003). As the wearer, I want the watch to warn me like an aircraft does. There are three alert levels: WARNING (red), CAUTION (amber) and ADVISORY (cyan). The most important active alert shows as a master annunciator on every page. Tapping it acknowledges the alert and stops repeating vibrations or sounds. Navigating pages and flipping toggles gives a crisp mechanical "switch click" feel instead of a generic buzz. When the battery falls below 15 % and is not charging, the watch raises a "STALL" WARNING with a heavy, pulsing stick-shaker vibration. I can pick vibration strength (off/low/high) and turn on a quiet mode that silences sounds but keeps vibration for warnings. Other features (time unsync, missing peripheral, timer end) use the same alert system. Acceptance: simulated battery levels raise and clear STALL correctly; with several alerts active the highest level wins; acknowledging stops the repeats; page swipes click; intensity "off" suppresses all haptics except STALL.
```

**/speckit.plan**
```text
/speckit.plan Follow AD-SAS-001 §5.3 (low-battery sequence) and §6. Pure src/core/alert_arbiter: an active-alert set keyed by AlertId with level, raised/acked state, and deterministic priority ordering. Unit-test it on native. AlertManager service (svc tick, core 0): raise/clear/ack API, posts AlertRaised/AlertCleared on the EventBus, maps AlertId → {haptic pattern, optional voice clip (S5 hook), repeat interval} from a constexpr table. HapticService: DRV2605 via IBoard::haptic(effectId) using the ROM library (effect 1 Strong Click / 4 Sharp Click for UI, and a pattern sequencer for the stick shaker, e.g. repeated effect 47/52 buzz with gaps every 2 s until acked), intensity from SettingsStore. PageManager calls HapticService::click() on page change. AnnunciatorBar widget in src/ui shown on all pages (top band), tap → ack. PowerService raises STALL (<15 %, not charging) and PULL_UP (≤5 %) with 2 % hysteresis. Retrofit S0 peripheral-failure cautions and the S2 TIME UNSYNC advisory to use AlertManager. Provide a serial command `ad alert raise <id>` for testing.
```

---

## S5 — GPWS audio ("Bitchin' Betty")

**/speckit.specify**
```text
/speckit.specify Voice warnings and cabin chimes for AeroDeck (slice S5, FR-AUD-001..005, NFR-PWR-003). As the wearer, I want the watch to speak like a cockpit warning system. A soft cabin chime plays at the top of every hour, except during quiet hours (default 22:00–07:00) or when quiet mode is on. Alerts play voice callouts: "Master Caution", "Pull Up" (critical battery), "Bingo Fuel" (timer finished), "Minimums" (generic alarm) and "Traffic" (radar). I can set the volume in at least four steps. The speaker amplifier must be powered only while something is playing, so audio costs no battery when silent. Sounds must not stutter the display. Voice clips are original or text-to-speech recordings, not copied from aircraft or films. Acceptance: the hourly chime plays at hh:00 outside quiet hours; raising each alert plays its callout once per raise (repeating per the alert's settings until acknowledged); the amplifier rail is measurably off when idle; the PFD keeps ≥30 FPS during playback.
```

**/speckit.plan**
```text
/speckit.plan Follow AD-SAS-001 §6–§7.1. Assets: data/audio/*.wav (16-bit PCM mono 16 kHz), uploaded with `pio run -t uploadfs` to the LittleFS partition from S0. Add a tools/ script documenting how the clips were generated (TTS command lines) so they are reproducible. AudioService: an `audio` FreeRTOS task on core 0 (priority 5) with a clip queue. On play: take RailLease(AudioAmp), mount LittleFS once, stream with ESP8266Audio 2.0.0 (AudioFileSourceLittleFS → AudioGeneratorWAV → AudioOutputI2S on the board's I2S pins exposed by IBoard; on Ultra these are the LilyGoLib Player/PCM amplifier pins under IDF 4.4), apply volume gain, release the lease after the queue drains plus a 200 ms tail. Chime scheduler in svc tick using TimeSnapshot and quiet hours from SettingsStore. Hook AlertManager's voice-clip mapping (from S4) to AudioService::play. Respect quiet mode. Verify the amp rail state with the PowerService profiling log, and FPS with the overlay during playback.
```

---

## S6 — G-Force & flight recorder

**/speckit.specify**
```text
/speckit.specify Activity "flight recorder" for AeroDeck (slice S6, FR-REC-001..005, FR-PFD-008). As the wearer, I want my daily steps converted to "Flight Miles" in nautical miles, using a stride length I can set (default 0.75 m; 1 NM = 1852 m). I also want the watch to record the highest g-force from sudden wrist movements as today's "Max G", with the time it happened. Both show on the PFD watch face. A Recorder MFD page shows today's nautical miles, steps, Max G and a 7-day bar chart. Counters reset at local midnight. History survives reboots, deep sleep and battery swaps. Acceptance: walking a measured distance gives NM within ±15 %; a sharp wrist flick raises Max G and records its time; after midnight the previous day appears in the 7-day chart; after a reboot today's values are kept (losing at most one hour of data if power is cut abruptly).
```

**/speckit.plan**
```text
/speckit.plan Follow AD-SAS-001 §6–§7.1. RecorderService (svc tick, core 0): steps via IBoard::stepCount() (BHI260AP step-counter virtual sensor on Ultra; BMA423 step counter on S3), peak g from AttitudeSnapshot.g_mag (≥50 Hz source from S1; track the maximum between ticks inside AttitudeService to avoid missing short spikes). Pure src/core/units (steps→NM, g formatting) and day-rollover logic with a 7-entry ring, unit-tested on native, including DST/TZ edge cases. Persistence: RTC_DATA_ATTR mirror updated every tick, NVS write hourly and on eject/sleep, schema-versioned. Publish RecorderSnapshot. Fill the PFD's reserved Max G / Flight Miles slots. RecorderPage uses an LVGL 9 lv_chart (bar) for 7 days plus readouts. Stride length is configurable on the Settings page.
```

---

## S7 — NVG mode, eject & power UX

**/speckit.specify**
```text
/speckit.specify Night mode, emergency sleep and power behaviour for AeroDeck (slice S7, FR-NVG-001..003, FR-EJT-001..004, NFR-PWR-002/004). As the wearer, I want an NVG toggle that turns the whole interface into pure black with monochrome phosphor green (#00FF66) and dims the screen to 10 %, on every page at once, persisting across reboots. Warnings must still stand out in NVG mode (blinking/inverse instead of red). I want a yellow-and-black striped "PULL TO EJECT" handle at the top of the screen. Double-tapping it and then swiping down saves my data, powers down the display, radio, speaker and GPS, and puts the watch into deep sleep within 2 seconds. Pressing the button (or tilting my wrist, if the hardware supports it) wakes it back to the PFD with my data intact. In normal use, the screen turns off after a configurable idle time (default 15 s) and wakes when I raise my wrist. Battery life should keep the average drain at or below 50 mA, about 22 hours of typical use with radios off (target redefined from two days in S7). This slice includes a measurement report. Acceptance: NVG applies everywhere, including live widgets; eject needs the full gesture (no accidental triggers); wake restores the recorder and timer state; screen timeout and raise-to-wake work; a power profile log and a measured battery-life estimate are written to docs/.
```

**/speckit.plan**
```text
/speckit.plan Follow AD-SAS-001 §5.3 (eject sequence), §8 (NVG) and ADR process. Theme: an NVG palette variant swapping all tokens; shared lv_style_t objects so restyling is global; send NvgChanged; brightness via IBoard::setBrightness (AMOLED brightness on Ultra, PWM backlight on S3) at 10 %. WARNING styling in NVG = blink animation + inverse. EjectHandle widget: hazard-stripe drawn with an lv_canvas or image, double-tap detection (≤400 ms) then a downward drag ≥40 % of the widget height. PowerService::eject(): broadcast EjectRequested, wait (≤500 ms) for services to flush (Recorder, Chrono to RTC RAM + NVS), stop radio/audio/mic/gnss tasks, setRail off for Display/Lora/AudioAmp/Gnss/Nfc via AXP2101 + XL9555, IBoard::configureWake (button via PMU IRQ/ext0, IMU interrupt only if Cap::ImuWakeInterrupt; verify BHI260AP INT routing on Ultra and resolve SRS OI-1 with ADR-010), esp_deep_sleep_start(). Screen timeout: display off + light sleep friendly, wrist-raise via BHI260AP wrist-tilt/any-motion virtual sensor or accel threshold fallback. Power profiling mode logs rail states and PMU current at 1 Hz. Produce docs/power_report.md with measured mA per state and a battery-life estimate against NFR-PWR-002.
```

---

## S8 — "Mach / Fuel" drop-tank chronograph

**/speckit.specify**
```text
/speckit.specify Drop-tank countdown timer for AeroDeck (slice S8, FR-CHR-001..005). As the wearer, I want a countdown timer pictured as an external drop tank that drains as time runs down, with a large MM:SS readout. I can choose presets (5, 15, 25, 50 minutes) or a custom time, and start, pause, resume and reset it. A Pomodoro mode alternates work and break intervals automatically. When the tank empties, the watch raises a "BINGO FUEL" caution with vibration and the "Bingo Fuel" voice callout. The timer keeps running, and still alerts on time, if I switch pages, the screen turns off, or the watch is put into deep sleep. Acceptance: a 1-minute timer fires within ±1 s of the target in all three cases (on page, other page, deep sleep); pause/resume keeps the remaining time exactly; Pomodoro cycles work → break → work with an alert at each transition.
```

**/speckit.plan**
```text
/speckit.plan Follow AD-SAS-001 §6–§7.1. Pure src/core/drop_tank_model: state machine (Idle, Running, Paused, Expired, Pomodoro phases) driven by absolute end-epoch time, not tick counting. Unit-test it on native, including pause/resume and phase transitions. ChronoService (svc tick): uses TimeSnapshot UTC epoch, mirrors state in RTC_DATA_ATTR, and before deep sleep/eject registers an esp_sleep_enable_timer_wakeup for the remaining time so expiry wakes the device and raises the alert. Publish ChronoSnapshot and ChronoExpired → AlertManager BINGO_FUEL (haptic + S5 voice). ChronoPage: tank widget (rounded body with a fill level animated through lv_anim, NVG-aware colours), MM:SS in the HUD font, preset roller, start/pause/reset buttons ≥48 px.
```

---

## S9 — TCAS LoRa radar

**/speckit.specify**
```text
/speckit.specify TCAS tactical radar using LoRa for AeroDeck (slice S9, FR-TCS-001..008, NFR-REG-001, NFR-SEC-003). As the wearer, I want a radar MFD page with a sweeping plan-position display. Nearby AeroDeck watches or LoRa nodes appear as green "bogeys", placed on range rings by signal strength (stronger = closer). Tapping a bogey shows its callsign, signal strength, signal quality, last-seen age and battery. While TCAS is enabled, my watch broadcasts a small beacon with my chosen callsign every 30 s (adjustable) and listens in the background. Targets disappear after 5 minutes of silence. A new target entering the inner ring raises a "TRAFFIC" advisory. Radio region, frequency, power and modulation are configurable but limited to what is legal in the selected region (default EU868), and the watch enforces the transmit duty-cycle limit. Beacons carry no personal data beyond the callsign (and position only if I opt in later). The radio is powered only while TCAS is enabled. Acceptance: two watches see each other within one beacon interval; moving them apart moves the bogey outward; turning TCAS off powers the radio down; the duty-cycle budget is never exceeded in a 1-hour soak test.
```

**/speckit.plan**
```text
/speckit.plan Follow AD-SAS-001 §6 and §7.2–§7.3. RadioService: a `radio` FreeRTOS task on core 0 using RadioLib 7.x SX1262 (Module from the board HAL; on Ultra the RF switch is controlled through the XL9555 expander, so reuse LilyGoLib initLoRa wiring), RailLease(Lora) held only while TCAS is enabled, interrupt-driven RX on DIO1 with startReceive/readData, a TX queue gated by src/core/duty_cycle (sliding-window airtime budget per region, airtime computed from SF/BW/CR/payload). Region table with allowed frequencies/power (EU868 default 869.525 MHz SF9 BW125 CR4/5 14 dBm; US915). Pure src/core/beacon_codec implementing the v1 packet in SAS §7.2 and src/core/rssi_range (ring mapping + EMA). Unit-test both on native. TcasService: beacon scheduling with ±10 % jitter, target table (max 16, ageing), TargetNew events, TRAFFIC advisory via AlertManager (+ "Traffic" clip). TcasPage: PPI widget with range rings, a sweep line animated by lv_anim (rotation transform), bogeys as small objects at a stable hash-based bearing, and a tap → detail popover. Settings: callsign (≤8 chars), region, interval, enable toggle. Leave a hook for passive Meshtastic decode (FR-TCS-007) unimplemented.
```

---

## S10 — PTT: dB meter & acoustic commands ("Vox Populi")

**/speckit.specify**
```text
/speckit.specify Push-to-talk microphone page for AeroDeck (slice S10, FR-PTT-001..003, FR-PTT-005). As the wearer, I want a PTT MFD page with a large hold-to-talk button. The microphone is active only while I hold it, and nothing is recorded or stored. In dB-meter mode, holding the button shows ambient sound level as a cockpit-style gauge, updating at least 10 times per second, with a peak-hold marker. In command mode, holding the button listens for simple acoustic gestures, a whistle or a double clap, which trigger configurable actions (for example: toggle NVG, open Radar, start the timer). Spoken keywords ("Night Mode", "Radar", "Timer") are a stretch goal if feasible on the device. Acceptance: the gauge responds to sound changes in real time and drops to idle when released; a whistle and a double clap are recognised in ≥8 of 10 tries in a quiet room with ≤1 false trigger per minute of normal conversation; the microphone is provably off when the button is not held.
```

**/speckit.plan**
```text
/speckit.plan Follow AD-SAS-001 §6. MicService: a `mic` FreeRTOS task on core 0, started on PTT press and stopped on release, RailLease(Mic) if the board gates it. PDM capture through the board HAL (on Ultra under IDF 4.4 the LilyGoLib PDM class; 16 kHz mono, 16-bit, 512-sample frames). Pure src/core/audio_features: RMS → dBFS with A-weighting optional, peak-hold decay, FFT (use esp-dsp or kissfft vendored; record the choice in ADR-011), whistle detector (narrow-band peak 1–3 kHz sustained ≥300 ms), double-clap detector (two broadband transients 150–600 ms apart). Unit-test the detectors on native with synthetic and recorded sample buffers checked into test/fixtures. Publish MicSnapshot{dbfs, peak}. Gesture → action mapping in SettingsStore, executed via EventBus. PttPage: hold button (LV_EVENT_PRESSED/RELEASED), arc/meter gauge, mode switch. Keyword spotting (e.g. ESP-SR/TFLite Micro) is out of scope unless a spike fits the memory budget. Record findings in research.md.
```

---

## S11 — IFF / countermeasures

**/speckit.specify**
```text
/speckit.specify IFF countermeasures page for AeroDeck (slice S11, FR-IFF-001..004, NFR-USE-003). As the wearer, I want a page with a red, guarded "FIRE / CHAFF" switch. I first flip open the safety cover, and only then can I press the trigger. The cover closes by itself after 10 seconds. On watches with an infrared LED, firing sends a pre-programmed remote-control code I have chosen (for example TV power or air-conditioner toggle). On the T-Watch Ultra, which has no infrared LED, firing performs an NFC action instead: either presenting a tag a phone can read (a URL or text I configure), or "interrogating" a tapped NFC tag and showing its identity. If the selected action isn't supported on my watch, the page says so clearly. Acceptance: the trigger cannot fire with the cover closed; the cover auto-closes; on Ultra a phone reads the configured NDEF content and a tapped tag's UID/type is shown; on S3 the IR code toggles a test device.
```

**/speckit.plan**
```text
/speckit.plan Follow AD-SAS-001 §4 (capabilities) and §6. IffService with two backends selected by board capability: IrBackend (Cap::IrTx, S3 only, IRremoteESP8266 C++20 fork already in env:twatchs3, code library in a constexpr table: NEC/Samsung/LG TV power + one A/C) and NfcBackend (Cap::Nfc, Ultra, ST25R3916 + NFC-RFAL fork already in env:twatch_ultra, build flag USING_ST25R3916, RailLease(Nfc) only during an action). Modes: NDEF card emulation of a configured URI/Text record, if RFAL card-emulation support proves workable on this stack (spike first; otherwise downgrade to reader mode and record ADR-012), and reader mode: poll for NFC-A/B/F/V for 10 s and report UID, technology and NDEF content if present. GuardedSwitch widget: a red-striped cover with an flip animation, trigger enabled only when open, a 10 s auto-close timer, click haptic. IffPage shows an unsupported banner based on caps(). Settings: selected IR code / NDEF payload.
```

---

## S12 — GNSS enhancements (Ultra)

**/speckit.specify**
```text
/speckit.specify GPS features for AeroDeck on the T-Watch Ultra (slice S12, FR-GPS-001/002, FR-PFD-010, FR-TIM-005, FR-TCS-005). As the wearer, I want a GPS MFD page showing fix status, number of satellites, position, ground speed in knots and course. Optionally (a setting), the PFD's heading tape shows my true course over ground when moving, with seconds moved to a small numeric readout. When Wi-Fi isn't available, GPS time can correct the clock. When both watches have GPS fixes, the TCAS radar plots targets at their true bearing and distance instead of estimating from signal strength. The GPS receiver is powered only while one of these features needs it. Acceptance: outdoors, a fix is obtained and shown; the course-on-heading-tape option tracks walking direction; with Wi-Fi off and a drifted clock, a GPS fix corrects it; two GPS-equipped watches show each other at the correct bearing within ±20°.
```

**/speckit.plan**
```text
/speckit.plan Follow AD-SAS-001 §6 and §7.2. GnssService: RailLease(Gnss), NMEA from the board GPS UART (LilyGoLib GPS on Ultra) parsed with TinyGPSPlus (already in lib_deps), a reference-counted consumer API (GpsPage, PFD course option, TimeService fallback, TCAS position), publish GnssSnapshot{fix, sats, lat, lon, sog_kt, cog, utc}. TimeService fallback: if no NTP sync for >24 h and a GNSS fix with valid date exists, set the system clock + RTC (optionally PPS-aligned via GPS_PPS pin 13). TCAS: set flags.bit0 and lat/lon in the beacon (opt-in setting); pure src/core/geo for haversine distance/bearing (unit-tested). The PPI switches a target to true bearing/scaled range when both positions exist. PFD: HeadingTape source switch (seconds vs COG when sog > 2 kt). Feature-flag AD_FEATURE_GNSS; on S3 (no Cap::Gnss) all of it compiles out cleanly.
```

---

## S13 — LoRa walkie-talkie (experimental)

**/speckit.specify**
```text
/speckit.specify Experimental LoRa voice messages for AeroDeck (slice S13, FR-PTT-004, FR-PTT-005, NFR-REG-001). As the wearer, I want a walkie-talkie mode on the PTT page. Holding the button records up to 3 seconds of speech. On release, the clip is compressed to a very low bitrate and sent over LoRa to my paired watch, which plays it through its speaker with a short radio "squelch" sound. Messages respect the radio duty-cycle limits. If a message can't be sent because of the limit, I'm told how long to wait. Nothing is stored after playback. This is an experiment: a feasibility spike decides whether it ships, and it stays behind a build-time switch. Acceptance: a 3-second clip is intelligible on the receiving watch at short range; the transmission respects the duty-cycle budget; packet loss of a fragment results in a truncated clip, not a crash.
```

**/speckit.plan**
```text
/speckit.plan Follow AD-SAS-001 §6 and §7.2 (packet type 0x02 voice-fragment). Start with a spike documented in research.md: build Codec2 (an ESP32 port, mode 1200 or 700C) for Arduino-ESP32 2.0.17, then measure encode/decode CPU time per 40 ms frame on core 0, RAM use (PSRAM allowed) and flash size. Go/no-go thresholds: encode faster than real time with ≥30 % headroom, internal heap stays ≥40 KB. If go: VoiceLinkService records via MicService (8 kHz downsample), encodes into a frame buffer (3 s ≈ 450 B at 1200 bps), fragments into ≤200 B LoRa payloads with seq/total header, sends through RadioService (duty-cycle gated; pre-check total airtime and report the wait time), and reassembles on receive with a 2 s gap timeout. Decode → AudioService raw PCM playback, prefixed with a squelch clip. Pairing = a peer callsign/node ID in settings. Compile-time flag AD_FEATURE_VOICELINK=0 by default. If no-go: record ADR-013 with the measurements and close the slice.
```

---

## Tips
- Run the slices in roadmap order. S2→S3 and S4→S5 depend on each other, while S6 can run in parallel with S2/S3 after S1.
- After `/speckit.plan`, check that `plan.md` lists the SRS IDs it covers and passes the Constitution Check.
- If a plan has to deviate from AD-SAS-001, add `docs/adr/NNN-*.md` and update the SAS ADR table in the same branch.
