# Feature Specification: Alert Manager & Haptics (Slice S4)

**Feature Branch**: `005-alert-haptics`

**Created**: 2026-11-11

**Status**: Draft

**Input**: User description: "S4"

**Traceability**: AD-SRS-001 FR-ALR-001…004, FR-HAP-001…003 · Roadmap milestone M2 (first slice) · Depends on S0
(merged; S1–S3 also merged and are retrofitted) · Voice callouts are hooks only here and are filled by slice S5 ·
Prompt source: `docs/speckit_slice_prompts.md` § S4

## User Scenarios & Testing *(mandatory)*

### User Story 1 - One master annunciator for everything that needs my attention (Priority: P1)

As the wearer, every problem the watch knows about shows in one place, like an aircraft's master warning panel. There
are three levels:
- **WARNING** (red): act now;
- **CAUTION** (amber): something is wrong;
- **ADVISORY** (cyan): for information.

The most important active alert is shown in a band at the top of every page. If several are active, I can see how
many there are and the band cycles through them. All existing problem messages use this one system: a missing
peripheral, settings reset, safe mode, clock not set, time not synchronised, low battery.

**Why this priority**: Every later feature (voice, timers, radio) reports through it, and it replaces the S0 caution
banner.

**Independent Test**: Raise alerts of different levels from the diagnostic console, then check which one the band shows,
its colour, the count, and the cycling. Clear them and check the band disappears.

**Acceptance Scenarios**:

1. **Given** no alert is active, **When** I look at any page, **Then** no annunciator band is shown.
2. **Given** an ADVISORY and a CAUTION are active, **When** a WARNING is raised, **Then** the band shows the WARNING in
   red within 1 second, with the number of other active alerts (e.g. "+2").
3. **Given** two alerts of the same level, **When** both are active, **Then** the one raised first is shown first
   (stable, predictable order).
4. **Given** several alerts are active and acknowledged, **When** I watch the band, **Then** it cycles through all of
   them (highest level first), so none is hidden.
5. **Given** an alert is active, **When** its cause goes away (e.g. the clock gets set), **Then** it disappears from
   the band within 2 seconds, and the next one (if any) is shown.
6. **Given** the watch boots with a missing peripheral, a reset of settings, or an unset clock, **When** the watch face
   appears, **Then** these show as CAUTIONs in the band (as in S0–S2), and TIME UNSYNC shows as an ADVISORY.
7. **Given** any page (PFD, WX, other pages, Settings), **When** an alert is active, **Then** the band is visible and
   stays inside the screen's rounded corners without covering the page's key readouts more than the S0 banner did.

---

### User Story 2 - Low battery is unmistakable: STALL and stick shaker (Priority: P1)

As the wearer, when the battery drops below 15 % and the watch is not charging, it raises a red **STALL** warning and
shakes like an aircraft's stick shaker: a heavy, pulsing vibration that repeats until I acknowledge it. At 5 % or
below, it escalates to **PULL UP**. When I plug in the charger, or the battery recovers, the warning clears by itself.

**Why this priority**: A dead watch is the worst failure of a daily watch. The roadmap's M2 goal is "Low battery is
unmistakable".

**Independent Test**: Simulate battery levels with the diagnostic console. Check that STALL appears below 15 %, PULL UP
at ≤5 %, the vibration pattern repeats until acknowledged, and both clear on charging or recovery (with hysteresis).

**Acceptance Scenarios**:

1. **Given** the battery is at 16 % and not charging, **When** it drops to 14 %, **Then** a STALL WARNING is raised
   within 2 seconds, and the stick-shaker pattern starts.
2. **Given** STALL is active and not acknowledged, **When** time passes, **Then** the stick-shaker pattern repeats about
   every 2 seconds until I acknowledge it.
3. **Given** STALL is active, **When** I acknowledge it, **Then** the vibration stops within 1 second, and STALL stays
   in the band (steady) until it clears.
4. **Given** STALL is active, **When** the battery drops to 5 %, **Then** PULL UP (WARNING) replaces STALL and the
   vibration starts again, even though STALL was acknowledged.
5. **Given** STALL or PULL UP is active, **When** the charger is connected, **Then** the warning clears within
   2 seconds and the vibration stops.
6. **Given** STALL cleared because the battery recovered, **When** the level wobbles around 15 %, **Then** STALL
   does not flicker. It clears only at ≥17 % (2 % hysteresis), and is raised again only below 15 %. PULL UP clears at
   ≥7 %, and STALL then stays (acknowledged) until ≥17 %.
7. **Given** haptic intensity is set to "off", **When** STALL is raised, **Then** the stick shaker still vibrates
   (warnings are never silenced).

---

### User Story 3 - Acknowledge to stop the nagging (Priority: P1)

As the wearer, I tap the annunciator band to acknowledge. All repeating vibrations stop. The alerts stay listed (steady
instead of flashing) until their cause is gone. A newly raised alert gets my attention again.

**Why this priority**: Without a way to silence a repeating alert, the alert system is unusable.

**Independent Test**: Raise a WARNING, tap the band, and check that the repetition stops, the band turns steady, and a
newly raised alert flashes and vibrates again.

**Acceptance Scenarios**:

1. **Given** an unacknowledged alert is shown (flashing), **When** I tap the band, **Then** all active alerts become
   acknowledged, the band stops flashing within 0.5 seconds, and repeating haptics stop.
2. **Given** all alerts are acknowledged, **When** a new alert is raised, **Then** the band flashes again and the new
   alert's haptic pattern plays.
3. **Given** an alert is acknowledged, **When** it clears and is raised again later, **Then** it is unacknowledged
   again (a new occurrence).
4. **Given** a computer is connected over USB, **When** I send the acknowledge command, **Then** the result is the same
   as tapping.
5. **Given** I tap the band, **When** the tap is registered, **Then** it doesn't also trigger a control or page swipe
   underneath.

---

### User Story 4 - A crisp switch click when I navigate (Priority: P2)

As the wearer, swiping between pages, pressing the side button to go home, and operating controls (switches, buttons,
picker OK) give a short, crisp "switch click" feel, like a cockpit toggle, instead of a generic buzz.

**Why this priority**: It makes the watch feel like an instrument, but it is not safety-relevant.

**Independent Test**: Swipe through all pages and operate the Settings controls. Each one gives exactly one short click.
With intensity "off", nothing clicks.

**Acceptance Scenarios**:

1. **Given** intensity is low or high, **When** a page change completes (swipe or button), **Then** one click is felt
   within 50 ms of the page settling.
2. **Given** intensity is low or high, **When** I operate a switch, button or picker confirmation, **Then** one click is
   felt.
3. **Given** a repeating WARNING pattern is playing, **When** I change page, **Then** the click doesn't interrupt or
   distort the warning pattern beyond one pattern cycle.
4. **Given** intensity is "off", **When** I navigate or operate controls, **Then** no click is felt.
5. **Given** fast repeated swipes, **When** several page changes happen within a second, **Then** each gives one click,
   and clicks never pile up into a long buzz.

---

### User Story 5 - Choose vibration strength and quiet mode (Priority: P2)

As the wearer, in Settings I choose the vibration strength (off / low / high) and turn **quiet mode** on or off. Quiet
mode is for meetings and night: it silences all sounds (when sound arrives in slice S5) and all CAUTION/ADVISORY
vibrations, but WARNING vibrations still play. Both settings are kept across reboots.

**Why this priority**: Comfort and social acceptability. The defaults already work.

**Independent Test**: Change the strength and check the click and caution strength. Turn on quiet mode, raise a CAUTION
(no vibration) and a WARNING (vibrates). Reboot and check that the settings are kept.

**Acceptance Scenarios**:

1. **Given** intensity "low", **When** a click or CAUTION pattern plays, **Then** it is noticeably weaker than at
   "high".
2. **Given** quiet mode is on, **When** a CAUTION or ADVISORY is raised, **Then** it shows in the band but doesn't
   vibrate.
3. **Given** quiet mode is on, **When** a WARNING is raised, **Then** it vibrates (and repeats until acknowledged).
4. **Given** quiet mode is on, **When** I navigate, **Then** UI clicks follow the intensity setting (quiet mode only
   affects alerts and sound).
5. **Given** I changed both settings, **When** the watch reboots, **Then** they are kept.

---

### Edge Cases

- **Haptic motor missing or failed** (peripheral status): alerts still show visually; a HAPTICS FAIL caution is
  raised (as in S0); nothing blocks or retries endlessly.
- **More alerts than the band can name:** the count shows the total; cycling covers all of them; at least 16
  simultaneous alerts are supported without loss.
- **The same alert raised twice:** it stays a single entry. A raise while already active doesn't re-trigger the haptic
  pattern. Only a **higher level** re-arms it (unacknowledged, pattern plays). A changed text just updates the message.
- **Alert raised and cleared within the same second:** it may never appear; no stale entry remains.
- **Acknowledge with nothing unacknowledged:** no effect, no click storm.
- **The watch sleeps or the screen is off** (later slices): unacknowledged WARNING vibration continues; visual cycling
  pauses.
- **Battery reading unavailable** (PMU read fails): no STALL/PULL UP is raised from invalid data.
- **Charging but still at a low level** (e.g. 3 % on the charger): no STALL/PULL UP (charging clears them).
- **Boot with low battery:** STALL is raised on the first valid battery reading after boot, not before the watch face
  appears.
- **Display:** the band text must fit the 410×502 rounded screen. Long messages are shortened to fit, never cut
  mid-character.
- **Test hooks:** alerts raised from the diagnostic console use the same levels and patterns as real ones.

## Requirements *(mandatory)*

### Functional Requirements

**Alert manager**
- **FR-001**: The watch MUST keep a set of active alerts. Each has an identifier, a level (WARNING, CAUTION, ADVISORY), a
  short message (≤24 characters), an acknowledged flag and the time it was raised. Raising an active alert updates it
  in place; clearing removes it. (FR-ALR-001)
- **FR-002**: The active alerts MUST be ordered by level (WARNING > CAUTION > ADVISORY), then by time raised (earlier
  first). The first is the **master** alert. The order MUST be deterministic for the same inputs.
- **FR-003**: Each alert type MUST map to a haptic pattern, an optional voice callout (filled in S5) and a repeat rule:
  - WARNING: repeat until acknowledged;
  - CAUTION: play once on raise;
  - ADVISORY: one short gentle pulse on raise.

  The mapping MUST be a single table that later features extend. (FR-ALR-002)
- **FR-004**: Every existing problem report MUST go through the alert manager with these levels:
  - CAUTION: peripheral failures ("<NAME> FAIL"), SETTINGS RESET, SETTINGS STORAGE ERROR, SAFE MODE, CLOCK NOT SET;
  - ADVISORY: TIME UNSYNC;
  - WARNING: the S1 LOW FUEL caution is replaced by STALL (FR-012).

**Master annunciator**
- **FR-005**: A band at the top of every page MUST show the master alert's message in its level colour (red / amber /
  cyan) with the count of other active alerts. While any alert is unacknowledged, it MUST flash (about 1 Hz). When all
  are acknowledged, it MUST be steady and cycle through all active alerts every 3 s. With no alerts, it MUST be hidden.
- **FR-006**: The band MUST update within 1 second of a raise, clear or acknowledge.

**Acknowledge**
- **FR-007**: Tapping the band MUST acknowledge all active alerts: repeating haptics stop within 1 second, and the band
  turns steady. The tap MUST NOT pass through to the page below. (FR-ALR-003)
- **FR-008**: An alert raised after an acknowledgement (new, or cleared and raised again, or escalated) MUST be
  unacknowledged and play its pattern.
- **FR-009**: The diagnostic console MUST offer commands to list active alerts, raise a test alert (id, level, text),
  clear one, and acknowledge all.

**Haptics**
- **FR-010**: Page changes (swipe or button) and control activations (switch, button, picker confirm) MUST play one
  crisp click within 50 ms. Clicks MUST NOT queue up: a new click replaces a pending one. (FR-HAP-001)
- **FR-011**: Haptic output MUST be prioritised: a WARNING pattern is never cut short by a click or a lower-level
  pattern. A click arriving during a WARNING pattern is dropped.
- **FR-012**: Battery below **15 %** and not charging MUST raise **STALL** (WARNING) with a heavy pulsing
  **stick-shaker** pattern (about 1.5 s of strong pulses, repeating about every 2 s until acknowledged). Battery at or
  below **5 %** and not charging MUST raise **PULL UP** (WARNING; it replaces STALL; same pattern, re-armed even if
  STALL was acknowledged). Both clear when charging, or when the battery recovers to ≥17 % (STALL) / ≥7 % (PULL UP).
  Between 7 % and 17 % after PULL UP clears, STALL remains, **already acknowledged** (stepping down doesn't nag again).
  (FR-HAP-002)
- **FR-013**: Haptic intensity MUST be selectable: off / low / high (default **low**), kept across reboots. It scales
  clicks and CAUTION/ADVISORY patterns. **WARNING patterns always play at full strength, even when intensity is off.**
  (FR-HAP-003)
- **FR-014**: Quiet mode (on/off, default off, kept across reboots) MUST suppress all audio (applies from S5) and
  CAUTION/ADVISORY haptics. WARNING haptics and UI clicks are not affected. (FR-ALR-004)
- **FR-015**: Settings MUST offer "Vibration" (off / low / high) and "Quiet mode" (on/off) on the watch, and both MUST
  be settable over the diagnostic console.

**Quality**
- **FR-016**: The alert ordering, acknowledge rules, repeat timing, battery thresholds with hysteresis, and the haptic
  priority/intensity rules MUST be tested off-device. (NFR-MNT-004)
- **FR-017**: Alerting and haptics MUST NOT reduce the PFD's smoothness (median ≥28 frames per second with an
  unacknowledged WARNING flashing and vibrating) or block the user interface.

### Key Entities

- **Alert**: identifier, level, message, acknowledged, raised time, haptic pattern and repeat rule (from the alert
  table), optional voice clip id (S5).
- **Alert table**: the static mapping from alert type to level, pattern, repeat rule and voice clip, extended by later
  slices (timer end, radio, …).
- **Haptic pattern**: a short sequence of vibration effects and pauses with a strength class (click, gentle, caution,
  stick shaker).
- **Haptic settings**: intensity (off/low/high), quiet mode (on/off). Persisted.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Over a battery simulation sweep 20 % → 3 % → 20 % (not charging, then charging), STALL is raised once
  below 15 %, PULL UP once at ≤5 %, and both clear correctly with no flicker, in 3 of 3 runs.
- **SC-002**: With 5 alerts of mixed levels active, the band shows the WARNING first, in 10 of 10 random raise orders.
- **SC-003**: After a tap on the band, repeating vibration stops within 1 second in 10 of 10 tries.
- **SC-004**: 20 of 20 page changes each give exactly one click (none missed, none doubled), and with intensity "off",
  0 of 20 click.
- **SC-005**: A wearer recognises a STALL warning (by feel alone, without looking) as different from a page click and
  from a CAUTION, in 3 of 3 blind tries.
- **SC-006**: The PFD stays at a median of ≥28 frames per second while an unacknowledged WARNING is flashing and
  vibrating.

## Assumptions

- **Hardware:** both supported watches have a haptic driver with a built-in effect library; patterns are built from its
  effects. Effect choices (e.g. sharp click, strong buzz) are design decisions for the plan.
- **Audio** (sounds, voice callouts, hourly chime) is slice S5. S4 only reserves a voice-clip slot per alert type, and
  the quiet-mode setting that S5 will honour.
- **Battery source:** the existing battery reading and charging state (S1 EICAS). The S1 LOW FUEL caution (same 15 %
  threshold) is replaced by STALL. The FUEL gauge's own colour cue on the PFD stays.
- **Acknowledge scope:** one tap acknowledges all active alerts (aircraft "master caution" behaviour). Per-alert
  acknowledgement is out of scope.
- **Alert history/log** beyond the active set is out of scope (the serial log keeps raise/clear lines).
- **Defaults:** intensity low; quiet mode off. Quiet hours by time of day come with S5 (chime).
- **The band** replaces the S0 caution banner in the same top area. Its height may grow slightly for the count and
  flashing state.
- **Sleep/screen-off** behaviour comes with slice S7. S4 assumes the screen is on.

## Dependencies

- S0 (merged): EventBus, settings storage, caution banner (replaced), peripheral status, diagnostic console.
- S1 (merged): battery/charging reading and the LOW FUEL logic (replaced by STALL/PULL UP), PFD.
- S2 (merged): TIME UNSYNC (moves to the alert manager as an ADVISORY).
- Later slices (S5 voice, S8 timer end, S9 radio) add alert types through the alert table.
