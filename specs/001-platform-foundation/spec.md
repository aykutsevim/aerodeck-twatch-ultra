# Feature Specification: Platform Foundation (Slice S0)

**Feature Branch**: `001-platform-foundation`

**Created**: 2026-11-11

**Status**: Draft

**Input**: User description: "S0 platform foundation"

**Traceability**: AD-SRS-001 FR-PLT-001…008, NFR-REL-001…003, NFR-MNT-001…003, NFR-PERF-003,
NFR-PERF-005, NFR-PWR-001 · Roadmap milestone M0 · Prompt source: `docs/speckit_slice_prompts.md` § S0

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Boot and navigate between cockpit pages (Priority: P1)

As the wearer, I power on (or reset) the watch and quickly land on a home screen. From there I
swipe left or right to move through a ring of full-screen pages, and the ring wraps at both ends.
One press of the physical button always brings me back home. A second press from home takes me
to the page I last visited. Each page change briefly shows which page I'm on. In this slice the
home screen is a simple placeholder for the future flight-display watch face, alongside two
placeholder pages and the Settings page.

**Why this priority**: Every later cockpit module (watch face, weather, radar, timer…) plugs into
this navigation. Without it nothing else can be shown or tested.

**Independent Test**: Flash the firmware, power-cycle, time the boot, then swipe through all
pages in both directions and use the button. Delivers a working, wearable shell.

**Acceptance Scenarios**:

1. **Given** the watch is off, **When** it is powered on, **Then** the home screen is visible and
   responsive within 4 seconds.
2. **Given** the home screen is shown, **When** I swipe left repeatedly, **Then** pages appear in
   the order Home → Page A → Page B → Settings → Home.
3. **Given** the home screen is shown, **When** I swipe right once, **Then** the Settings page
   appears (the ring wraps backwards).
4. **Given** any non-home page is shown, **When** I press the physical button once, **Then** the
   home screen appears.
5. **Given** I returned home from Page B with the button, **When** I press the button again,
   **Then** Page B appears.
6. **Given** any page change, **When** the new page appears, **Then** a page indicator shows the
   current position for about 1 second and then hides.

---

### User Story 2 - Settings that survive restarts (Priority: P1)

As the wearer, I open the Settings page and change a preference: screen brightness, whether the
diagnostics overlay is shown, or how detailed the diagnostic output is. After a reboot, or after
the battery runs flat and is recharged, my choices are still in effect. The Settings page also
shows read-only information: firmware version, board model, and the status of each peripheral.
I can restore all settings to defaults, but only after confirming.

**Why this priority**: Every later slice adds settings (time zone, Wi-Fi, radio region…). The
mechanism that keeps them, and the page that shows them, must exist first.

**Independent Test**: Change each available setting, power-cycle the watch, and confirm the values
persist and take effect. Then restore defaults and confirm they revert.

**Acceptance Scenarios**:

1. **Given** brightness is at the default, **When** I change it on the Settings page, **Then** the
   screen brightness changes at once.
2. **Given** I changed a setting, **When** the watch is power-cycled, **Then** the changed value is
   still shown and still in effect.
3. **Given** the watch loses power during a settings save, **When** it boots again, **Then** all
   settings hold either their previous or their new values, never corrupted or mixed values,
   and the watch boots normally.
4. **Given** settings have been changed, **When** I choose "Restore defaults" and confirm,
   **Then** every setting returns to its default. Cancelling the confirmation changes nothing.
5. **Given** the Settings page is open, **When** I view the About/Status section, **Then** I see the
   firmware version, the board model and an OK/FAILED/ABSENT state for each peripheral.

---

### User Story 3 - Graceful boot when hardware misbehaves (Priority: P1)

As the wearer, if an optional component (long-range radio, NFC, GPS, speaker, memory card) is
missing, broken or fails to start, the watch still boots to the home screen and works. A visible
caution tells me which component failed. It never crashes, freezes or restarts in a loop.

**Why this priority**: This is a constitution-level rule (Principle I). The watch is worn daily, and
a single faulty part must not turn it into a brick.

**Independent Test**: Use a diagnostic/test option that simulates the failure of each optional
component at boot, or physically disconnect one where possible. Confirm home is reached and the
caution is shown.

**Acceptance Scenarios**:

1. **Given** an optional component fails to start, **When** the watch boots, **Then** the home screen
   appears within 4 seconds and a caution names the failed component.
2. **Given** several optional components fail, **When** the watch boots, **Then** all failures are
   listed on the Settings status section, and the watch stays fully navigable.
3. **Given** a component failed at boot, **When** the watch runs for 1 hour, **Then** no restarts or
   freezes occur.
4. **Given** a core component needed to show anything (display or power management) fails,
   **When** the watch boots, **Then** the failure is reported on the diagnostic output and the
   watch does not restart in a loop.

---

### User Story 4 - Developer diagnostics (Priority: P2)

As the developer, I can turn on an on-screen overlay that shows the current screen refresh rate and
free memory. I also get timestamped, levelled diagnostic messages over the USB cable. Passwords
and keys are never printed.

**Why this priority**: Later slices have strict smoothness and memory budgets (≥30 frames per
second, ≥40 KB free memory). They can only be checked if measurement exists from day one.

**Independent Test**: Enable the overlay and read its values while swiping. Connect a serial
terminal, change the diagnostic detail level, and confirm the output changes.

**Acceptance Scenarios**:

1. **Given** the overlay is enabled, **When** pages are being shown or swiped, **Then** the refresh
   rate and free memory are displayed and update at least once per second.
2. **Given** the diagnostic level is set to "warnings only", **When** the watch runs normally,
   **Then** informational messages are not emitted.
3. **Given** any diagnostic level, **When** a setting marked secret is saved or loaded, **Then** its
   value never appears in the diagnostic output.

---

### User Story 5 - Reproducible build from a clean checkout (Priority: P2)

As the developer (or a future contributor), I clone the repository onto a fresh machine and build
the firmware for the T-Watch Ultra with one command. It succeeds with no manual file edits. The
secondary T-Watch S3 build also compiles, and the off-device logic tests run.

**Why this priority**: The current build only works because a third-party library was hand-patched
on one machine (Principle VII). Losing that patch silently breaks every future slice.

**Independent Test**: Delete all downloaded dependencies and build output, then build both board
targets and run the off-device tests from scratch.

**Acceptance Scenarios**:

1. **Given** a fresh clone with no cached dependencies, **When** the Ultra build command runs,
   **Then** it completes successfully with no manual steps.
2. **Given** a fresh clone, **When** the S3 build command runs, **Then** it compiles successfully.
3. **Given** a fresh clone, **When** the off-device test command runs, **Then** all tests pass.
4. **Given** a successful Ultra build, **When** it is flashed to the watch, **Then** User Stories 1–3
   behave as specified.

### Edge Cases

- **Rapid swipes:** quick consecutive swipes move exactly one page per swipe and never skip, freeze
  or leave half-drawn pages.
- **Button during swipe:** a button press during a page transition ends on the home screen.
- **Ambiguous gestures:** vertical or very short swipes don't change pages, and taps on page
  controls aren't mistaken for swipes.
- **Settings from older firmware:** saved settings from an older or newer layout are migrated, or
  reset to defaults with a caution, and never crash the watch.
- **Out-of-range values:** a stored value outside its allowed range is replaced by the default.
- **Full or damaged settings storage:** the watch boots with defaults and raises a caution.
- **Brightness at minimum:** the lowest brightness still leaves the screen visible indoors.
- **Low battery at boot:** boot still reaches home. Battery alerts are out of scope (slice S4).

## Requirements *(mandatory)*

### Functional Requirements

**Boot & navigation**

- **FR-001**: The watch MUST show an interactive home screen within 4 seconds of power-on or reset
  (SRS FR-PLT-001).
- **FR-002**: The watch MUST arrange pages in a fixed, wrapping ring. A horizontal swipe MUST
  move exactly one page in the swipe direction (SRS FR-PLT-002).
- **FR-003**: A single press of the physical button MUST return to home from any page. On home,
  a press MUST open the most recently visited non-home page, or the first page if none has been
  visited since boot (SRS FR-PLT-003).
- **FR-004**: Every page MUST be told when it becomes visible and when it stops being visible, and
  MUST stop using any hardware it no longer needs once it is hidden (SRS FR-PLT-004).
- **FR-005**: After each page change, the watch MUST show a page position indicator for about
  1 second (SRS FR-PLT-007).
- **FR-006**: This slice MUST ship a placeholder home page, two placeholder pages (A and B) and a
  Settings page. Later slices MUST be able to add pages without changing the navigation
  behaviour.

**Settings**

- **FR-007**: The watch MUST persist user settings across reboots, power loss and deep sleep
  (SRS FR-PLT-005).
- **FR-008**: The Settings page MUST let the wearer view and change every user-editable setting
  that exists in this slice: display brightness, diagnostics overlay on/off, and diagnostic
  detail level (SRS FR-PLT-006).
- **FR-009**: Each setting MUST have a defined default and allowed range. Out-of-range stored
  values MUST be replaced by the default.
- **FR-010**: A setting save MUST be all-or-nothing. A power loss during a save MUST leave either
  the old or the new value (SRS NFR-REL-003).
- **FR-011**: The watch MUST support a stored-settings format version and handle settings saved by
  an older or newer format without crashing (migrate or reset to defaults with a caution).
- **FR-012**: The wearer MUST be able to restore all settings to defaults, with a confirmation step.
- **FR-013**: Settings marked secret (to be added by later slices, e.g. Wi-Fi password) MUST never be
  displayed in full or written to diagnostic output (SRS NFR-SEC-001).
- **FR-014**: The Settings page MUST show read-only firmware version, board model, and a status
  (OK / FAILED / ABSENT) for every peripheral.

**Resilience & power**

- **FR-015**: Failure or absence of any optional peripheral at boot MUST NOT stop the watch from
  reaching a usable home screen. Each failure MUST be shown to the wearer as a caution naming the
  component (SRS NFR-REL-001).
- **FR-016**: The watch MUST recover automatically from a hung main activity (watchdog). It MUST
  NOT enter a restart loop when a peripheral is permanently faulty (SRS NFR-REL-002).
- **FR-017**: Optional power-hungry components MUST stay powered off unless a feature currently
  needs them. In this slice no feature needs them, so they MUST all be off after boot
  (SRS NFR-PWR-001).
- **FR-018**: Background work MUST NOT make the visible interface pause for longer than one frame
  (SRS NFR-PERF-003).

**Diagnostics**

- **FR-019**: The watch MUST emit timestamped diagnostic messages over USB with selectable levels
  (errors, warnings, info, debug) (SRS FR-PLT-008).
- **FR-020**: The watch MUST provide an optional on-screen overlay showing the refresh rate and
  free memory, updated at least once per second.
- **FR-021**: The watch MUST provide a diagnostic way (e.g. a test option) to simulate the failure
  of each optional peripheral at boot, so that FR-015 can be verified without disassembly.

**Build & portability**

- **FR-022**: The firmware for the T-Watch Ultra MUST build from a clean checkout with a single
  command and no manual edits, including any needed fixes to third-party code (SRS NFR-MNT-003).
- **FR-023**: The firmware MUST also compile for the T-Watch S3 (SRS NFR-MNT-005).
- **FR-024**: Off-device automated tests for hardware-independent logic MUST exist and pass with a
  single command (SRS NFR-MNT-004).
- **FR-025**: Screen layouts MUST adapt to the board's screen size and shape, with no layout that
  only works on one board's resolution (SRS NFR-MNT-002).
- **FR-026**: The existing "hello world" demo application MUST be replaced by this foundation.

### Key Entities

- **Page**: a full-screen view in the navigation ring. It has a name, a position in the ring, a
  visible/hidden state, and hardware needs that apply only while it is visible.
- **Setting**: a named user preference with a type, default, allowed range, secret flag and
  current value. Grouped under a settings format version.
- **Peripheral status**: per hardware component: name, whether the board has it, and its startup
  result (OK / FAILED / ABSENT).
- **Caution**: a notice shown to the wearer that names a problem (e.g. "LoRa radio failed").
  In this slice it is a simple notice. The full priority-based alert system arrives in slice S4.
- **Diagnostic message**: timestamp, level, source and text. Never contains secret values.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In 10 of 10 cold boots, the home screen is interactive within 4 seconds.
  *(Amended 2026-11-11 from 3 s: the measured floor on the T-Watch Ultra is ~3.45 s, made up of ~1.2 s ROM/PSRAM
  startup plus fixed display-reset, panel and 32 kHz-crystal init inside LilyGoLib. See validation.md §1.)*
- **SC-002**: In a test of 100 consecutive swipes (50 each direction), 100 % land on the expected
  page, with no skipped, frozen or half-drawn pages.
- **SC-003**: The button returns to home in 100 % of 20 presses from random pages, within
  0.5 seconds.
- **SC-004**: Every editable setting keeps its value across 10 power cycles, including 3 cycles
  where power is cut during a save.
- **SC-005**: With each optional peripheral failing in turn (simulated), the watch reaches home in
  100 % of boots and names the failed component. Zero restarts occur in a 1-hour run.
- **SC-006**: Page transitions finish in 0.3 seconds or less. The frame rate during transitions is
  measured and recorded (no pass threshold in S0).
  *(Amended 2026-11-11: the ≥30 FPS target moves to slice S1, where the constitution requires it for the PFD
  horizon (NFR-PERF-001) and rendering performance is tuned. S0 measured 15–28 FPS, see validation.md §5.)*
- **SC-007**: Free memory shown by the overlay never drops below 40 KB during a 30-minute session
  of continuous navigation.
- **SC-008**: A contributor with a fresh clone builds the Ultra firmware successfully on the first
  attempt with one command (verified once by deleting all cached dependencies).
- **SC-009**: The S3 build compiles and the off-device test suite passes from a fresh clone.

## Assumptions

- The primary device is the T-Watch Ultra (502×410 screen). The T-Watch S3 only needs to compile in
  this slice; on-device S3 testing is out of scope.
- The "physical button" is the watch's user-accessible side button. If a short press is reserved
  by hardware for power functions, the closest available press gesture is used, and the plan
  documents it.
- The home screen is a placeholder (e.g. time and title). The real flight-display watch face is
  slice S1.
- Placeholder pages A and B exist only to test navigation. Later slices replace them.
- Brightness has at least 5 user-selectable steps. The minimum step stays visible indoors.
- Defaults: brightness at a mid-high level, overlay off, diagnostic level "info".
- Sleep, screen-off timeout, night mode, alerts with vibration/sound, and time synchronisation are
  out of scope (slices S2, S4, S7). Cautions in this slice are visual only.
- Settings are edited on the watch. Provisioning over the USB cable arrives with slice S2
  (Wi-Fi credentials).
- The firmware version string is taken from the build (e.g. a version tag or commit identifier).
