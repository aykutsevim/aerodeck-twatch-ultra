# Feature Specification: NVG Mode, Eject & Power UX (Slice S7)

**Feature Branch**: `008-nvg-eject-power`

**Created**: 2026-11-12

**Status**: Draft

**Input**: User description: "S7 on new feature branch"

**Traceability**: AD-SRS-001 FR-NVG-001…003, FR-EJT-001…004, NFR-PWR-002, NFR-PWR-004 (also NFR-USE-003, FR-REC-005
deep-sleep part, FR-PLT-001 wake, SRS OI-1) · Roadmap milestone M3 (first slice) · Depends on S4 (alerts), S6
(recorder save-on-demand) · Prompt source: `docs/speckit_slice_prompts.md` § S7

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Screen turns off when idle and wakes when I raise my wrist (Priority: P1)

As the wearer, I want the screen to turn off by itself after a short idle time (default 15 s), and to come back on
when I raise my wrist to look at it or press the button, so the battery lasts and the watch still feels instant.

**Why this priority**: Today the screen never turns off; it is the largest battery consumer and the main lever for
the two-day battery target (NFR-PWR-002). It is useful on its own without the other stories.

**Independent Test**: Leave the watch untouched for the idle time and check that the screen goes dark; raise the
wrist as when reading a watch and check that the watch face is back within 1 second.

**Acceptance Scenarios**:

1. **Given** the default timeout (15 s) and no touch or button input, **When** 15 s pass, **Then** the screen turns
   off (within 1 s of the timeout).
2. **Given** the screen is off and raise-to-wake is on, **When** I raise my wrist and turn the screen towards me,
   **Then** the screen turns on within 1 s showing the page I left, with live, up-to-date values.
3. **Given** the screen is off, **When** I press the button, **Then** the screen turns on; this press only wakes the
   screen (it does not also jump to the watch face).
4. **Given** the screen is off, **When** I touch the screen, **Then** nothing happens (touches on a dark screen are
   ignored, so a sleeve cannot operate the watch).
5. **Given** I keep touching or swiping, **When** the timeout would otherwise expire, **Then** each touch restarts
   the idle time.
6. **Given** I choose another timeout (10 s, 15 s, 30 s, 1 min, 5 min, or Never) or turn raise-to-wake off in
   Settings, **When** the watch restarts, **Then** my choices are kept.
7. **Given** the screen is off, **When** a caution or warning alert is raised, **Then** the screen turns on to show it
   (vibration and voice behave as before), and the normal idle time then applies again.

---

### User Story 2 - Pull-to-Eject: emergency deep sleep (Priority: P1)

As the wearer, I want a yellow-and-black striped "PULL TO EJECT" handle at the top of the watch face. Double-tapping
it and then pulling it down saves my data, switches everything off, and puts the watch into deep sleep within
2 seconds. Pressing the button (or tilting my wrist, if the hardware supports it) wakes the watch back to the
watch face with my data intact.

**Why this priority**: It is the slice's signature cockpit feature and the way to stretch the battery when the watch
is not needed (FR-EJT-001…003). It also closes the recorder's "survives deep sleep" requirement from S6.

**Independent Test**: Note the recorder values, perform the full eject gesture, confirm the watch is dark and draws
deep-sleep current, press the button, and check that the watch face returns with the same recorder values.

**Acceptance Scenarios**:

1. **Given** the watch face, **When** I look at the top of the screen, **Then** a yellow/black hazard-striped handle
   labelled "PULL TO EJECT" is visible and does not cover the attitude display's essential readouts.
2. **Given** the handle, **When** I double-tap it, **Then** it shows an armed state (clear visual change) for up to
   3 s; **When** I then drag it down far enough within those 3 s, **Then** the watch ejects.
3. **Given** the handle is armed, **When** 3 s pass without the pull, or I let go before pulling far enough,
   **Then** it disarms and nothing else happens.
4. **Given** I only single-tap, only swipe down, or swipe left/right across the handle, **When** the gesture ends,
   **Then** the watch does not eject (no accidental triggers; NFR-USE-003).
5. **Given** a completed eject gesture, **When** the watch ejects, **Then** today's recorder data is saved, the
   display is in its sleep state, the speaker, radio and GPS supplies are off, and the watch is in deep sleep within
   2 s of the end of the pull.
6. **Given** the watch is ejected, **When** I press the button, **Then** it boots to the watch face within 4 s
   (FR-PLT-001) with today's steps, Max G (and its time) and the 7-day history unchanged.
7. **Given** the hardware can wake from a wrist movement, **When** I tilt my wrist while ejected, **Then** the watch
   wakes as with the button; if the hardware cannot, this is documented and only the button wakes it.
8. **Given** a setting I changed (e.g. NVG on), **When** the watch wakes from eject, **Then** the setting is kept.

---

### User Story 3 - NVG night mode (Priority: P2)

As the wearer, I want an NVG toggle that turns the whole interface into pure black with monochrome phosphor green
(#00FF66) and dims the screen to 10 %, on every page at once, kept across restarts. Warnings must still stand out in
NVG mode.

**Why this priority**: It makes night use comfortable (M3 exit: "Night use is comfortable"), but the watch is fully
usable without it.

**Independent Test**: Toggle NVG in Settings, then swipe through every page and check that all text, lines,
gauges and live widgets are green on black and the screen is dim; raise a warning and check that it blinks.

**Acceptance Scenarios**:

1. **Given** NVG is off, **When** I turn it on, **Then** within 1 s every page (watch face, weather, recorder,
   settings), the alert annunciator, overlays and the eject handle use only black and phosphor green shades, and the
   screen brightness drops to 10 %.
2. **Given** NVG is on, **When** live values change (horizon, tapes, clock, recorder chart, weather tiles), **Then**
   they keep being drawn in green on black (no element falls back to its day colour).
3. **Given** NVG is on, **When** a WARNING is raised, **Then** it is shown inverse (black on green) and blinking
   instead of red; a CAUTION is shown inverse without blinking; ADVISORY is a green outline on black. All three remain
   distinguishable at a glance.
4. **Given** NVG is on, **When** I turn it off, **Then** the normal colours and my previous brightness come back.
5. **Given** NVG is on, **When** the watch restarts or wakes from eject, **Then** it starts in NVG with 10 %
   brightness from the first frame of the watch face.
6. **Given** NVG is on, **When** I change the brightness setting, **Then** the setting is stored for day use but
   the screen stays at the NVG level.

---

### User Story 4 - Power profile and battery-life report (Priority: P2)

As the developer, I want a power-profiling mode that logs the supply states and battery current once per second,
and a written report with the measured current in each state and a battery-life estimate against the ≤50 mA / ≥22 h target,
so the battery claim is measured, not promised.

**Why this priority**: The roadmap's M3 exit requires a measured battery life (NFR-PWR-002, NFR-PWR-004), but it
depends on stories 1–2 being in place to measure.

**Independent Test**: Turn on the profiling mode over the console, put the watch through each state (screen on,
screen off, NVG, ejected), and check that the log shows one line per second with the supply states and current, and
that the report lists a value for each state.

**Acceptance Scenarios**:

1. **Given** the console, **When** I turn profiling on, **Then** one line per second shows the battery current (or,
   where the watch cannot measure it, the estimated drain rate), battery voltage and level, charging state, screen on/off, NVG, and which optional supplies are on; **When** I
   turn it off, the lines stop.
2. **Given** the measurements, **When** the report is written, **Then** it lists the average current for: watch
   face on (day), watch face on (NVG), screen off idle, and ejected (deep sleep), the method used, and a
   battery-life estimate for a stated typical day.
3. **Given** the estimate, **When** it is compared with the ≤50 mA / ≥22 h target, **Then** the report states plainly whether
   the target is met, and if not, the main consumers and next steps.

---

### Edge Cases

- **Eject while charging (USB connected):** eject still works; the watch sleeps (charging continues by hardware).
- **Eject during audio or vibration:** playback and vibration stop; eject is not delayed by them beyond the 2 s
  budget.
- **Eject while Wi-Fi sync or the setup portal is active:** the network activity is abandoned; credentials already
  saved are kept; the next sync happens after wake on the normal schedule.
- **Eject with an active alert:** alerts are not carried through sleep; after wake, conditions are re-evaluated
  (e.g. a still-low battery raises STALL again).
- **Recorder save fails at eject** (e.g. storage error): the watch still sleeps within 2 s; the failure is logged;
  at most the data since the last periodic save is lost.
- **Steps while ejected:** steps walked while in deep sleep are not counted unless the motion sensor can keep
  counting through sleep; this is documented, not hidden.
- **Wake by anything other than the button or wrist** (e.g. USB plugged in): if it wakes the watch, it boots
  normally; this is acceptable.
- **Timer state:** the countdown timer arrives in S8; S7 provides the step in the eject sequence where it will save
  its state and set its wake-up, and wake from a timer is verified in S8.
- **Raise-to-wake false triggers** (e.g. walking with arms swinging): occasional false wakes are acceptable; the
  screen goes dark again after the timeout. The report notes how often it happened in the measured day.
- **Screen off with no motion sensor:** raise-to-wake is unavailable; the button wakes the screen; Settings shows
  raise-to-wake as unavailable.
- **"Never" timeout:** the screen stays on; this is the old behaviour and is not counted in the battery estimate.
- **Setup portal / Wi-Fi setup screen:** the screen does not time out while the setup screen is shown.
- **Low battery STALL / PULL UP alarm** (S4): turns the screen on like any warning.
- **NVG and the hourly chime/haptics:** unchanged (NVG affects display only).
- **Display smoothness:** NVG and the screen-off logic must not reduce the watch face's smoothness (see SC-007).

## Requirements *(mandatory)*

### Functional Requirements

**Screen timeout and raise-to-wake**
- **FR-001**: The screen MUST turn off after a configurable idle time without touch or button input: 10 s, 15 s
  (default), 30 s, 1 min, 5 min, or Never. The choice MUST be settable on the watch and over the console and kept
  across restarts and eject. (FR-EJT-004)
- **FR-002**: With the screen off, raising the wrist to the viewing position MUST turn it on within 1 s when
  raise-to-wake is on (default on, settable, kept). A button press MUST also turn it on; that press MUST NOT perform
  its normal action. Touches on a dark screen MUST be ignored, including a touch still held when the screen comes
  back on (it MUST NOT act as a tap).
- **FR-003**: When the screen turns on again it MUST show the page that was shown before, with current values.
  Background work (time, recorder, alerts, chime, weather schedule) MUST continue while the screen is off.
- **FR-004**: A newly raised (or re-armed) caution or warning MUST turn the screen on. Updates to an alert that is
  already shown (e.g. changed text) MUST NOT wake the screen.
- **FR-005**: The screen MUST NOT time out while the Wi-Fi setup screen is shown.

**Pull-to-Eject**
- **FR-006**: The watch face MUST show a yellow/black hazard-striped "PULL TO EJECT" handle at the top of the
  screen. (FR-EJT-001)
- **FR-007**: Eject MUST require a double-tap on the handle (two taps within 0.4 s) followed, within 3 s, by a
  downward pull of the handle over at least 40 % of its travel. Any other gesture, a timeout or letting go early
  MUST disarm it without effect. The armed state MUST be clearly visible. (FR-EJT-001, NFR-USE-003)
- **FR-008**: On eject the watch MUST save the recorder state, stop audio, vibration and network activity, put
  the display into its sleep state (its supply stays on, because cutting it raises the deep-sleep current), switch
  off the speaker amplifier, radio and GPS supplies, set up its wake sources and enter deep sleep within 2 s
  of completing the gesture. (FR-EJT-002)
- **FR-009**: The eject sequence MUST provide a step where later features (the S8 timer) save their state and
  request a wake-up time before sleep.
- **FR-010**: The button MUST wake the watch from eject. A wrist movement MUST also wake it if the hardware supports
  it; whether it does MUST be determined on the device and recorded (resolves SRS OI-1). (FR-EJT-003)
- **FR-011**: After wake the watch MUST boot to the watch face within 4 s with the recorder data and all settings
  as they were before eject. (FR-EJT-003, FR-PLT-001, FR-REC-005)
- **FR-012**: The console MUST offer an eject command equivalent to the gesture (for testing) and MUST report the
  cause of the last wake at boot.

**NVG mode**
- **FR-013**: An NVG setting MUST switch the whole interface to a black background with phosphor green (#00FF66)
  and darker shades of that same green only (every colour mapped by brightness onto the black → #00FF66 ramp), and set the screen brightness to 10 % of maximum. (FR-NVG-001)
- **FR-014**: NVG MUST apply within 1 s to every page, the annunciator, overlays, the eject handle and live widgets,
  without restarting, and MUST be kept across restarts and eject (applied from the first frame after boot).
  (FR-NVG-002)
- **FR-015**: In NVG, WARNING MUST be shown inverse and blinking, CAUTION inverse and steady, ADVISORY as a green outline on black,
  so the levels stay distinguishable without colour. (FR-NVG-003)
- **FR-016**: Leaving NVG MUST restore the normal colours and the stored brightness; changing brightness while in NVG
  MUST store the value without leaving the NVG level.
- **FR-017**: NVG MUST be switchable from the Settings page and the console.

**Power profiling and report**
- **FR-018**: A power-profiling mode, switched on and off from the console, MUST log once per second the battery
  current (where the hardware measures it; otherwise the estimated drain rate), voltage, level, charging state,
  screen state, NVG state and the on/off state of each optional supply. (NFR-PWR-004)
- **FR-019**: A power report in `docs/` MUST record the measured average current for: watch face on (day), watch
  face on (NVG), screen off idle, and ejected; the measurement method; a typical-day usage profile; and the
  resulting battery-life estimate compared with the ≤50 mA / ≥22 h target (average drain ≤50 mA, ≥22 h on 1100 mAh), stating
  plainly whether it is met. (NFR-PWR-002; target redefined from ≥48 h during S7)

**Quality**
- **FR-020**: The eject gesture recognition, the idle/timeout state logic and the NVG alert-styling rules MUST be
  tested off-device. (NFR-MNT-004)
- **FR-021**: In day mode and in NVG, the watch face MUST keep a median of ≥30 frames per second (constitution IV),
  and the new logic MUST NOT lower the median by more than 2 frames per second against the slice's starting baseline.
- **FR-022**: Restoring the settings defaults MUST set NVG off, the timeout to 15 s, raise-to-wake on and the
  horizon tilt to 30°.

**Horizon reference** *(added during implementation at the wearer's request, SRS FR-PFD-012)*
- **FR-023**: The horizon MUST read level at a wearer-selected face tilt from the ground: 0°–90° in 15° steps
  (0° = level when lying flat, the S1 behaviour; 90° = level when held upright; default 30°, the natural reading
  angle). The setting MUST be on the Settings page and the console and kept across restarts and eject. The heading
  tape keeps showing the seconds: the watch has no magnetometer, so a true compass bearing is not possible (I2C scan
  and hub firmware checked during S7).

### Key Entities

- **Display power state**: on or off, the time of the last user input, and why the screen last turned on (raise,
  button, alert, touch).
- **Eject handle state**: idle, armed (with the time it was armed), pulling (with how far), ejecting.
- **Wake cause**: what woke the watch from eject (button, wrist motion, timer, other), reported at boot.
- **New settings**: NVG on/off (default off), screen timeout (default 15 s), raise-to-wake on/off (default on),
  horizon tilt (0°–90°, default 30°).
- **Power sample**: battery current, voltage, level, charging, screen on/off, NVG, optional supplies on/off, once
  per second.
- **Power report**: per-state average current, method, usage profile, estimate vs. target.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In 10 of 10 trials, the screen turns off within 1 s after the set idle time; in at least 9 of 10
  deliberate wrist raises, it turns on within 1 s.
- **SC-002**: In 10 of 10 full eject gestures, the watch is in deep sleep within 2 s; in 20 attempts of incomplete
  or wrong gestures (single tap, swipe only, slow double tap, pull after 3 s, left/right swipes) it ejects 0 times.
- **SC-003**: After 5 of 5 eject/wake cycles, the watch face is shown within 4 s of the button press, and today's
  steps, Max G (with its time), the history and the settings are unchanged.
- **SC-004**: With NVG on, a walk through every page finds no element in a non-green colour (wearer check), the
  screen is at 10 % brightness, and the wearer can tell WARNING, CAUTION and ADVISORY apart in a dark room.
- **SC-005**: The power report lists measured currents for all four states and a battery-life estimate; with the
  default timeout and radios off, the estimate is compared with the ≤50 mA / ≥22 h target and the result is stated (met or not met,
  with the gap).
- **SC-006**: Current in the ejected state is at least 10 times lower than with the screen off idle (deep sleep is
  real, not just a dark screen).
- **SC-007**: With NVG on and in day mode, the watch face keeps a median of ≥30 frames per second, no more than 2
  below the baseline measured at the start of the slice.

## Assumptions

- **Handle placement**: the eject handle is on the watch face only (the button returns to the watch face from any
  page), in the band at the very top, so it never hides the attitude display. While an alert is shown, the alert
  bar on the watch face sits just below the handle (over the heading row) so both stay usable; the heading tape,
  MAX G and FLT NM (which share that row) are hidden until the alert clears (MAX G and NM stay visible on the
  Recorder page); other pages are unchanged. *(Added by the plan, research R8.)*
- **Battery current**: the T-Watch Ultra's power chip has no battery-current sensor, so the profile logs voltage,
  level and the estimated drain rate, and the report derives currents from timed discharge (and a USB meter if
  available). *(Added by the plan, research R9.)*
- **NVG switch location**: Settings page and console. A faster shortcut (e.g. voice or gesture) may come in S10.
- **Brightness at 10 %** means 10 % of the panel's maximum brightness setting; the panel's lowest usable level may
  limit it, which is then noted in the report.
- **Wake from deep sleep is a full restart** of the watch (that is how the chip's deep sleep works); restoring "state"
  means the saved recorder data and settings, not the exact screen content.
- **Raise-to-wake** relies on the motion sensor; its sensitivity is tuned on the device. On boards without the motion
  sensor it is unavailable.
- **Wrist-motion wake from deep sleep** depends on the board wiring (SRS OI-1); the fallback is button-only wake (and
  timer wake in S8).
- **Battery-life estimate** follows the roadmap principle "measure, then promise": currents are measured on the
  device (from the power chip's battery reading, cross-checked over a multi-hour discharge where practical) and the
  estimate is calculated for a typical day. The target (redefined after the measurement from ≥48 h to an average drain ≤50 mA, ≥22 h) may turn out not to be met; the report says so.
- **T-Watch S3** must compile; features missing there (e.g. AMOLED-specific dimming, IMU wake) degrade gracefully
  (constitution VII).
- **Out of scope:** the countdown timer (S8) and its wake-up, always-on display, ambient-light dimming, scheduled
  automatic NVG, light sleep of the processor while the screen is off (may be adopted if measurements show a need).

## Dependencies

- S4 (merged): alert levels and annunciator for NVG styling and wake on alert.
- S6 (merged): recorder save-on-demand called before deep sleep.
- S5 (merged): audio service to stop before sleep.
- S2 (merged): network activity to abandon on eject; clock kept in the real-time clock across sleep.
- S0 (merged): settings storage, supply switching, console, page ring, theme.
- S8 (later): timer state save and timer wake-up in the eject sequence.
