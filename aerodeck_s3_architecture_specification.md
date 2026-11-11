# AeroDeck S3: Glass Cockpit & Tactical Avionics Watch Specification
**Target Hardware:** LilyGO T-Watch S3 (ESP32-S3, 915 MHz SX1262 LoRa)  
**Display & Graphics:** 240x240 ST7789V LCD, LVGL 8.3 (Double-Buffered via 8MB PSRAM @ 60 FPS)

**AeroDeck S3** is a comprehensive, multi-page **MFD (Multi-Function Display)** smartwatch firmware concept designed to utilize nearly every onboard hardware peripheral of the LilyGO T-Watch S3 (BMA423 IMU, SX1262 915 MHz LoRa, AXP2101 PMIC, DRV2605L Haptic Driver, MAX98357A I2S Amplifier, PDM Microphone, IR Transmitter, and FT6336U Capacitive Touch).

---

## 1. Display Architecture: PFD (Primary Flight Display) Main Watch Face

Instead of a traditional analog or digital watch face, the main screen replicates a modern fighter/commercial aircraft **PFD (Primary Flight Display)** and **ADI (Attitude Director Indicator / Artificial Horizon)**:

* **Dynamic Artificial Horizon (Live IMU Integration):** Using real-time *Pitch* and *Roll* angles calculated from the **BMA423** 3-axis accelerometer, the central blue/brown horizon line rotates and shifts dynamically as you tilt your wrist. Holding your wrist completely flat ("Level Flight" — $0^\circ$ pitch/roll) illuminates a green **"LOCK"** indicator at the center of the crosshair.
* **Scrolling Tape Timekeeping (Speed & Altitude Tapes):** Modeling aircraft HUDs, the left vertical *Airspeed Tape* displays the **Hours (00–23)**, while the right vertical *Altitude Tape* displays the **Minutes (00–59)**. Seconds are represented on the top **Heading Tape (Compass Rose)**, scrolling smoothly from $0^\circ$ to $360^\circ$ ($1\text{ sec} = 6^\circ$).
* **UTC / Zulu & Local Dual-Time:** Aviation-standard **ZULU (UTC)** time and local time are anchored in the bottom corners using a high-contrast green 7-segment HUD font.
* **Engine / System Telemetry (EICAS):** Along the bottom bar, real-time telemetry from the **AXP2101 PMIC** is mapped to aircraft engine gauges:
  * Battery Percentage $\rightarrow$ **"FUEL QTY (%)"**
  * Instantaneous Current Draw (mA) $\rightarrow$ **"FUEL FLOW"**
  * Internal Die Temperature $\rightarrow$ **"EGT (Exhaust Gas Temp - $^\circ\text{C}$)"**

---

## 2. Hardware-Mapped Avionics Modules & Creative Features

Swiping left/right across the touchscreen (cycling through MFD pages) or pressing the physical **Crown Button** activates dedicated functional avionics modules:

| Cockpit Module | Hardware Utilized | Function & Creative Implementation |
| :--- | :--- | :--- |
| **TCAS / Tactical Radar** | **SX1262 (915 MHz LoRa)** | Turns the watch into a functional RF radar. Periodically listens for or broadcasts LoRa beacon packets in the background. Nearby LoRa nodes (a home base station or other Meshtastic/LoRa devices) are plotted as green targets ("Bogeys") on a sweeping **PPI (Plan Position Indicator)** radar screen based on their **RSSI** and **SNR** signal strength. |
| **Stick Shaker & Flaps** | **DRV2605L (Haptic Engine)** | Replaces generic vibration buzzes with the DRV2605L's built-in library of **123 distinct haptic waveforms**. Swiping menus triggers a crisp mechanical cockpit toggle feel (*Sharp Click #1*), while dropping below 15% battery triggers a heavy, pulsing **"STALL! Stick Shaker"** alert on your wrist. |
| **Bitchin' Betty & GPWS** | **MAX98357A (I2S Speaker)** | Aircraft voice warning system (**GPWS**). Plays an authentic cabin *"Chime"* on the hour, and triggers `.wav` voice callouts stored in LittleFS/SPIFFS (e.g., *"Pull Up! Terrain, Terrain!"*, *"Master Caution"*, or *"Minimums"*) during alarms or critical system alerts. |
| **Vox Populi (ATC Push-to-Talk)** | **PDM Digital Microphone** | Holding down the on-screen **"PTT (Push-to-Talk)"** button activates the microphone for three modes:<br>1. **Voice Command (Edge AI / FFT):** Recognizes simple keywords (*"Night Mode"*, *"Radar"*, *"Timer"*) or whistle/clap frequencies.<br>2. **Cockpit dB Meter:** Visualizes ambient noise levels as a live cabin pressure / decibel gauge.<br>3. **LoRa Walkie-Talkie (Codec2):** Compresses 2–3 seconds of speech into ultra-low-bitrate (1200 bps) audio packets and transmits them over 915 MHz LoRa to a paired device. |
| **IFF / Countermeasures** | **IR LED (Infrared Transmitter)** | Flipping open a virtual red-guarded **"FIRE / CHAFF"** safety cover on the touchscreen and pressing the trigger blasts pre-programmed infrared codes to toggle TVs, air conditioners, or audio systems in the room. |
| **G-Force & Flight Recorder** | **BMA423 (FIFO + Step Counter)** | Logs daily step counts as **"Flight Miles (NM - Nautical Miles)"**. Also tracks peak acceleration from rapid wrist movements (e.g., during workouts or sudden reflexes) and logs the peak value as a **"Max G-Load"** readout on the watch face. |
| **METAR Weather Radar** | **ESP32-S3 Wi-Fi + PCF8563 RTC** | Connects to Wi-Fi once an hour (or on demand) to synchronize the **PCF8563 RTC** via NTP with millisecond precision and fetches live aviation weather (**METAR**) from an API (e.g., CheckWX / AviationWeather) for your local airport (e.g., **LTAC**), displaying real-time wind speed (Knots), wind direction, and QNH barometric pressure. |

---

## 3. User Experience (UX) & Special Interaction Scenarios

1. **NVG (Night Vision Goggles) Mode:** Flipping a virtual *"NVG Toggle"* switch shifts the entire UI color palette to a pure black background with **MIL-STD-3009** compliant monochromatic phosphor green (`#00FF66`) and dims the PWM display backlight to 10%.
2. **"EJECT" (Emergency Deep Sleep) Mechanism:** Double-tapping and swiping down on the yellow-and-black striped top bezel (*"Pull to Eject"*) saves critical flight logs to RTC RAM, instructs the **AXP2101** to cut power rails (ALDO/BLDO) to the display, LoRa radio, and audio amplifier, and puts the ESP32-S3 into **Deep Sleep**—waking up only via the BMA423 *Tilt-to-Wake* interrupt.
3. **Chronograph: "Mach / Fuel Timer":** A Pomodoro or workout countdown timer visualized as an **"External Drop Tank"**. As time counts down, the fuel gauge depletes, triggering a *"Bingo Fuel!"* voice and haptic alert when reaching zero.

---

## 4. Software Stack & Architecture Requirements

To build this firmware cleanly and modularly using **PlatformIO (VS Code)**, the following software stack and architectural practices are recommended:

### Core Libraries & Environment
* **Development Environment:** PlatformIO (`board = lilygo-t-watch-s3`, `framework = arduino` or `esp-idf`).
* **`LilyGoLib` (Official BSP):** Handles AXP2101 power rails, display pinouts, and sleep modes out of the box.
* **`LVGL (v8.3.x)`:** Drives the artificial horizon (Canvas/Image rotation), scrolling tapes, and needle gauges (`lv_meter`). Ensure `LV_COLOR_DEPTH 16` and `LV_MEM_CUSTOM 1` (for PSRAM allocation) are enabled in `lv_conf.h`.
* **`RadioLib`:** Provides reliable, interrupt-driven asynchronous packet reception and transmission for the SX1262 LoRa transceiver.
* **`SensorLib` (by Lewis He):** Manages the BMA423 accelerometer, PCF8563 RTC, FT6336U touch controller, and DRV2605L haptic driver.
* **`ESP8266Audio` or `Arduino-Audio-Tools`:** Streams `.wav` cockpit sound effects from LittleFS/SPIFFS to the MAX98357A via I2S.
* **`IRremoteESP8266`:** Drives the onboard IR LED (GPIO2) for remote control payloads.

### Architecture & Power Management Best Practices
* **Dual-Core FreeRTOS Task Pinning:**
  * **Core 1 (Application Core):** Dedicated strictly to LVGL UI rendering (`lv_timer_handler()`) and polling the BMA423 IMU to keep the artificial horizon running at a locked 60 FPS.
  * **Core 0 (Protocol Core):** Handles background tasks including LoRa (SX1262) packet listening, Wi-Fi METAR HTTP requests, and I2S audio playback so network or I/O latency never stutters the display animations.
* **AXP2101 Power Gating:** Both the MAX98357A audio amplifier and SX1262 LoRa module draw noticeable quiescent current even when idle. Dynamically disabling their respective LDO power rails via the AXP2101 when audio isn't playing and the TCAS Radar page isn't active will extend battery life from 4–5 hours to 2–3 days.