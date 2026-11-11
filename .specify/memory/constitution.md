<!--
Sync Impact Report
==================
Version change: 1.0.1 → 1.0.2
Bump rationale: PATCH: factual hardware detail added to Technical Constraints; no rule changed.
  - T-Watch Ultra screen corners are rounded with a 64 px radius (reported by the user in slice S1). UI layout
    keeps content inside the visible area (existing Principle VII resolution-independence rule applies).

Modified principles: none
Modified sections:
  Technical Constraints: Hardware (screen: 410×502 portrait, rounded corners r = 64 px)
Added sections: none
Removed sections: none

Templates reviewed (not modified, per command scope):
  ✅ .specify/templates/plan-template.md: no conflict
  ✅ .specify/templates/spec-template.md: no conflict
  ✅ .specify/templates/tasks-template.md: no conflict

History:
  1.0.1 (2026-11-11): screen corrected to 410×502 portrait (LilyGoLib rotation 0); LVGL pin corrected to 9.6.0.
  1.0.0 (2026-11-11): initial ratification (8 principles, Technical Constraints, Workflow & Quality Gates, Governance).

Deferred TODOs: none
-->

# AeroDeck Constitution

## Core Principles

### I. Every Slice Ships

- Every feature slice (S0–S13 in `docs/product_roadmap.md`) MUST end as flashable, wearable
  firmware that builds and runs on the primary target.
- The watch MUST always boot to a usable PFD watch face, even when an optional peripheral
  (LoRa, NFC, GNSS, audio, microSD) is missing or fails to initialise. Peripheral failures MUST
  surface as CAUTION alerts, never as crashes, asserts or boot loops.
- A slice MUST NOT be merged if it leaves any previously shipped slice broken.

**Rationale:** The device is worn daily. A broken watch blocks both use and further development.

### II. Layered Architecture Is Law

- Code MUST follow the AD-SAS-001 layering: UI → Services → Core logic → Board HAL → vendor
  libraries. A layer MUST depend only on layers beneath it.
- Only the UI task MAY call LVGL. Services MUST communicate with the UI through `Snapshot<T>`
  publications and the EventBus. Pages MUST command services only through their thread-safe APIs.
- Pages and services MUST NOT use LilyGoLib, SensorLib, RadioLib or other driver types
  directly. Hardware access goes through `IBoard` and its capability flags.
- Board-selection `#ifdef`s MUST live only in `src/board/`. Features MUST branch on
  `IBoard::caps()`, not on board macros.
- Every access to the shared I2C bus MUST hold the I2C mutex.

**Rationale:** This keeps the UI smooth, lets one codebase serve the T-Watch Ultra and T-Watch S3,
and keeps features independently replaceable.

### III. Battery Is a Feature

- Power-hungry peripherals (LoRa, audio amplifier, GNSS, NFC, microphone, Wi-Fi) MUST be powered
  only while a consumer holds a `RailLease`. Rails MUST be switched only by `PowerService`.
- Wi-Fi MUST NOT stay associated. It is brought up for a transaction and turned off afterwards.
- Every feature that keeps a radio or amplifier active MUST define its duty cycle in its plan.
- Battery-life claims MUST be backed by measurement (NFR-PWR-002 target: ≥48 h typical use with
  radios off).

**Rationale:** A watch that dies by afternoon is unusable, however good its features.

### IV. Frame Time Is Sacred

- No blocking I/O (network, filesystem, radio, audio, long I2C transactions) MAY run on the UI
  task. Such work MUST run in service tasks on core 0.
- The PFD horizon MUST sustain ≥30 FPS on the T-Watch Ultra (410×502 portrait). 60 FPS is a goal and
  MUST NOT be claimed without measurement.
- Free internal heap MUST stay ≥40 KB in every state. LVGL draw buffers and large assets MUST
  live in PSRAM.
- Any change that touches rendering MUST be checked with the FPS/heap debug overlay before merge.

**Rationale:** The artificial horizon is the product's signature. Stutter destroys the illusion.

### V. Test Off-Device First

- Pure logic (unit conversion, METAR parsing, alert arbitration, attitude math, beacon codec,
  RSSI ranging, duty-cycle budget, timer models, audio feature detectors) MUST live in
  `src/core/` with no Arduino or ESP-IDF dependency, and MUST have Unity tests that run with
  `pio test -e native`.
- New core logic MUST land with its tests in the same change. Tests SHOULD be written before the
  implementation.
- Every slice's `tasks.md` MUST include on-device acceptance checks derived from its spec's
  acceptance scenarios, run on real hardware before the slice is declared done.

**Rationale:** Flashing hardware is slow. Host tests catch logic bugs in seconds.

### VI. Deliberate, Compliant, Private Actions

- Every destructive or transmitting action (eject/deep sleep, IR/NFC fire, LoRa voice transmit)
  MUST need a deliberate gesture: a guard cover, a hold, or a double action.
- LoRa use MUST be region-compliant (default EU868). Frequency, power and duty cycle MUST be
  limited by the selected region, and the duty-cycle budget MUST be enforced in software before
  every transmission.
- The microphone MUST be active only while PTT is held. Audio MUST NOT be stored persistently by
  default.
- Wi-Fi credentials and API keys MUST be stored only in NVS and MUST NEVER be logged. HTTPS MUST be
  used wherever the remote API supports it.
- Radio packets MUST NOT carry personal data beyond the user-chosen callsign and opt-in position.
- Voice and audio assets MUST be original or TTS-generated. Copyrighted avionics or film audio
  MUST NOT be used.

**Rationale:** The watch transmits, listens and stores secrets. Trust and legality are non-negotiable.

### VII. Reproducible, Portable Builds

- `pio run -e twatch_ultra` MUST succeed from a clean checkout (no `.pio/` directory) with no
  manual steps, including any required vendor-library patching.
- `pio run -e twatchs3` MUST compile. Board-specific features compile out cleanly through
  capabilities and feature flags.
- All library and platform versions MUST be pinned exactly in `platformio.ini`. Version changes
  MUST be deliberate and noted in the plan.
- UI layout MUST be resolution-independent. Sizes go through the Theme scale `S(px)`, and
  board-specific coordinates MUST NOT appear outside the layout/theme module.

**Rationale:** Floating dependencies have already broken the build once (LilyGoLib 0.2.1).
Portability keeps the S3 path open.

### VIII. Simplicity & Explicit Experiments

- Abstractions MUST be justified by a current requirement, not speculation (YAGNI).
- Experimental features (Codec2 voice link, on-device keyword spotting, NFC card emulation) MUST
  start with a time-boxed spike recorded in `research.md`, MUST sit behind compile-time feature
  flags, and MUST default to off until promoted.
- Complexity that violates a principle MUST be recorded in the plan's Complexity Tracking table
  with the simpler alternative that was rejected.

**Rationale:** This is a hobby-paced project with many features. Unchecked complexity stalls it.

## Technical Constraints

- **Hardware:** Primary target is the LilyGO T-Watch Ultra (ESP32-S3, 16 MB flash, 8 MB PSRAM,
  410×502 portrait AMOLED with rounded corners (r = 64 px; 502×410 panel before LilyGoLib's default rotation), BHI260AP, PCF85063A, AXP2101, DRV2605, MAX98357A, PDM mic, SX1262, GNSS,
  ST25R3916 NFC). Secondary target is the T-Watch S3.
- **Toolchain:** PlatformIO, `framework = arduino`, `platform = espressif32@6.12.0`
  (Arduino-ESP32 2.0.17 / ESP-IDF 4.4).
- **Libraries:** LilyGoLib 0.2.0 (exact pin), LVGL 9.6.0, RadioLib 7.x, SensorLib 0.3.x,
  XPowersLib 0.3.x, ESP8266Audio 2.0.0. The concept spec's LVGL 8.3 reference is superseded.
- **Tasks:** UI + attitude on core 1. Network, radio, audio, microphone and service ticks on
  core 0 (AD-SAS-001 §5.1).
- **Storage:** Settings in NVS (namespace `aerodeck`, schema-versioned, atomic writes). Assets in a
  project-owned LittleFS partition. Partition tables MUST NOT reference files inside `.pio/`.
- **Canonical documents:** `docs/software_requirements_specification.md` (AD-SRS-001, the *what*)
  and `docs/software_architecture_specification.md` (AD-SAS-001, the *how*). Where they conflict,
  this constitution wins, then the SAS for design questions, then the SRS for behaviour questions.

## Development Workflow & Quality Gates

1. **Slice order:** Slices follow `docs/product_roadmap.md`. A slice MUST NOT start before its
   listed dependencies are merged.
2. **Specify:** Each `spec.md` MUST list the SRS requirement IDs it covers. New requirements
   MUST be added to the SRS (with a new ID) in the same branch.
3. **Plan:** Each `plan.md` MUST pass the Constitution Check against all eight principles and MUST
   cite the AD-SAS-001 sections it implements. Any deviation from the SAS MUST add an ADR in
   `docs/adr/NNN-title.md` and update the SAS ADR table in the same branch.
4. **Merge gates (all REQUIRED):**
   - `pio run -e twatch_ultra` and `pio run -e twatchs3` succeed.
   - `pio test -e native` passes.
   - On-device acceptance checks in `tasks.md` are executed and ticked.
   - Rendering changes verified at ≥30 FPS with ≥40 KB free heap.
   - No secrets in logs or committed files.
5. **Milestone gates:** The milestone definitions of done in the roadmap MUST be met before the
   next milestone's slices begin. M3 additionally needs `docs/power_report.md` with measured data.

## Governance

- This constitution supersedes plan preferences, coding habits and agent defaults. When a spec,
  plan or task conflicts with it, the constitution wins until it is formally amended.
- **Amendments:** Proposed in a dedicated change that updates this file, its Sync Impact Report,
  and any affected canonical document (SRS, SAS, roadmap). Each amendment MUST be noted in the
  roadmap's change history or ADR log.
- **Versioning (semantic):** MAJOR for removing or redefining a principle; MINOR for adding a
  principle or materially expanding a section; PATCH for clarifications and wording.
- **Compliance review:** Every `/speckit-plan` run MUST include a Constitution Check.
  `/speckit-analyze` SHOULD be run before `/speckit-implement`. Each merge MUST confirm the quality
  gates above. Unjustified complexity is grounds to reject a change.
- **Runtime guidance:** `docs/speckit_slice_prompts.md` provides per-slice prompts. AD-SAS-001
  provides design detail.

**Version**: 1.0.2 | **Ratified**: 2026-11-11 | **Last Amended**: 2026-11-11
