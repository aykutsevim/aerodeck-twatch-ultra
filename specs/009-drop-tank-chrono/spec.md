# Feature Specification: Drop-Tank Chronograph (Slice S8)

**Feature Branch**: `009-drop-tank-chrono`

**Created**: 2026-11-13

**Status**: Draft

**Input**: User description: "S8 on new feature branch"

**Traceability**: AD-SRS-001 FR-CHR-001…005 · Roadmap milestone M3 (last slice) · Depends on S4 (alerts and
haptics), S5 (voice callouts, chime), S7 (screen timeout, eject sequence with its pre-sleep step and timer wake) ·
Prompt source: `docs/speckit_slice_prompts.md` § S8

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Countdown with a draining drop tank (Priority: P1)

As the wearer, I want a countdown timer pictured as an external drop tank that drains as time runs down, with a large
MM:SS readout. I choose a preset (5, 15, 25 or 50 minutes) or a custom time, and I can start, pause, resume and reset
it.

**Why this priority**: It is the slice's core value and works on its own: a usable kitchen/focus timer in the
cockpit style.

**Independent Test**: Open the timer page, pick 5 minutes, start it, and check that the readout counts down second by
second while the tank level falls proportionally; pause, wait, resume and check that no time was lost or gained.

**Acceptance Scenarios**:

1. **Given** the timer page, **When** I pick the 25-minute preset, **Then** the readout shows 25:00 and the tank is
   full.
2. **Given** a preset is selected, **When** I start the timer, **Then** the readout counts down once per second and
   the tank level equals the remaining fraction of the set time (e.g. half full at 12:30 of 25:00).
3. **Given** a running timer, **When** I pause it, wait 30 seconds and resume, **Then** the remaining time after
   resuming equals the remaining time when I paused (to the second).
4. **Given** a running or paused timer, **When** I reset it, **Then** it stops and shows the set time with a full
   tank.
5. **Given** the timer page, **When** I set a custom time (1–120 minutes in 1-minute steps), **Then** that time is
   used like a preset.
6. **Given** the timer is idle, **When** I look at the page, **Then** the start control is clearly available, and
   while running, pause and reset are available.

---

### User Story 2 - BINGO FUEL when the tank runs dry (Priority: P1)

As the wearer, when the tank empties I want the watch to raise a "BINGO FUEL" caution with vibration and the "Bingo
Fuel" voice callout, wherever I am in the interface, so I cannot miss it.

**Why this priority**: A timer that does not alert is useless. FR-CHR-003.

**Independent Test**: Start a 1-minute timer and check that at zero the caution, vibration and voice appear together,
on the timer page and on any other page.

**Acceptance Scenarios**:

1. **Given** a running timer, **When** it reaches 00:00, **Then** the watch raises a BINGO FUEL caution with its
   vibration pattern and the "Bingo Fuel" callout within 1 second of the target time.
2. **Given** the caution is shown, **When** I acknowledge it (tap the alert bar), **Then** vibration and voice stop.
   The timer page shows an empty tank and "BINGO" until I reset or start again.
3. **Given** I reset or start the timer again, **When** a BINGO FUEL caution is active, **Then** the caution is
   cleared.
4. **Given** quiet mode or quiet hours are on, **When** the timer expires, **Then** the alert follows the existing
   rules (quiet hours suppress the caution voice; vibration still happens; quiet mode silences sound).

---

### User Story 3 - It keeps counting when I look away, the screen sleeps, or I eject (Priority: P1)

As the wearer, I want a running timer to keep counting and still alert on time if I switch pages, if the screen turns
off, or if I put the watch into deep sleep with Pull-to-Eject. In that last case the watch wakes up by itself at zero
to alert me.

**Why this priority**: The roadmap's exit criterion is "Bingo Fuel fires even after sleep" (FR-CHR-004). Without it
the timer cannot be trusted together with S7's power features.

**Independent Test**: Run three 1-minute timers: one staying on another page, one with the screen off, one ejected.
Each must alert within 1 second of its target time.

**Acceptance Scenarios**:

1. **Given** a running timer, **When** I swipe to another page, **Then** it keeps counting, and the alert fires on
   time on that page.
2. **Given** a running timer, **When** the screen turns off after the idle timeout, **Then** the alert fires on time
   and turns the screen on (S7 alert wake).
3. **Given** a running timer, **When** I eject (deep sleep), **Then** the watch wakes by itself at the target time,
   boots to the watch face and raises BINGO FUEL within 1 second of the target time.
4. **Given** I eject and press the button before the timer ends, **When** the watch wakes, **Then** the timer is
   still running with the correct remaining time.
5. **Given** a paused timer, **When** I eject and wake later, **Then** it is still paused with the same remaining
   time.
6. **Given** a running timer, **When** the watch restarts (update, crash, or console restart), **Then** the timer
   continues with the correct remaining time. If the target time passed while the watch was off, the alert is raised
   at boot with a note that it is late.

---

### User Story 4 - Pomodoro mode (Priority: P2)

As the wearer, I want a Pomodoro mode that alternates work and break intervals automatically, with an alert at each
transition, so I can run focus sessions without touching the watch.

**Why this priority**: It is a Should (FR-CHR-005) that builds on the countdown.

**Independent Test**: Start Pomodoro with short test intervals (e.g. 1-minute work, 1-minute break) and check that it
cycles work → break → work, alerting at each change, and counting cycles.

**Acceptance Scenarios**:

1. **Given** Pomodoro mode, **When** I start it, **Then** a work interval (default 25 minutes) runs, labelled WORK,
   with the tank draining.
2. **Given** a work interval ends, **When** it reaches zero, **Then** the watch raises BINGO FUEL (vibration + voice)
   and a break interval (default 5 minutes) starts automatically, labelled BREAK.
3. **Given** a break ends, **When** it reaches zero, **Then** the watch raises a "BREAK OVER" caution (vibration +
   chime) and the next work interval starts automatically.
4. **Given** Pomodoro is running, **When** I look at the page, **Then** I see the current phase and the number of
   completed work intervals.
5. **Given** Pomodoro is running, **When** I pause, resume or reset, **Then** they behave as for the countdown (reset
   ends the session).
6. **Given** Pomodoro is running, **When** I eject, **Then** the watch wakes at the next transition, alerts, and the
   next interval runs from the correct time.

---

### Edge Cases

- **Clock changes while running** (NTP sync, time zone change, manual clock set): the remaining time must not jump.
  A time zone change has no effect. A sync that corrects the clock by a few seconds may shift the target by at most
  that correction, never by hours.
- **Clock not set**: the timer still works, because it counts real elapsed time from the watch's own clock.
- **Expiry while an alert is already shown** (e.g. STALL): BINGO FUEL joins the alert list under the existing
  priority rules.
- **Eject with 6 seconds or less left**: the watch does not go to sleep (the eject is postponed). It stays awake and
  alerts on time. With more time left, it wakes early enough to finish booting before the target, and never alerts
  late by more than 1 second.
- **Battery runs flat while ejected with a timer**: no alert is possible; after charging, the timer shows as expired
  ("late") on boot.
- **Very long ejects**: a timer of up to 120 minutes must survive deep sleep with the same ±1 s accuracy.
- **Button press during a timer wake boot**: no conflict; the watch boots and the alert is shown.
- **Starting a new timer while BINGO FUEL is shown**: the caution is cleared and the new timer runs.
- **Changing the preset while running**: not allowed (selection disabled while running or paused; reset first).
- **Display off while expiring**: the alert turns the screen on (S7).

## Requirements *(mandatory)*

### Functional Requirements

**Countdown**
- **FR-001**: The timer MUST offer presets of 5, 15, 25 and 50 minutes and a custom time from 1 to 120 minutes in
  1-minute steps, and MUST allow start, pause, resume and reset. (FR-CHR-001)
- **FR-002**: The timer page MUST show the remaining time as MM:SS (updated every second) and as a drop-tank gauge
  whose fill equals the remaining fraction of the interval. (FR-CHR-002)
- **FR-003**: Pause and resume MUST keep the remaining time exact to the second; reset MUST return to the set time
  with a full tank.
- **FR-004**: The preset and custom choices MUST be locked while the timer runs or is paused.

**Alert**
- **FR-005**: At zero the watch MUST raise a BINGO FUEL caution with its vibration pattern and the "Bingo Fuel"
  callout within 1 s of the target time, following the existing quiet-mode and quiet-hours rules. (FR-CHR-003)
- **FR-006**: Acknowledging the caution MUST stop vibration and voice; resetting or starting the timer MUST clear it.

**Continuity**
- **FR-007**: A running timer MUST keep counting and alert on time on any page, with the screen off, and after Pull-
  to-Eject. When ejected, the watch MUST wake by itself at the target time (±1 s) and raise the alert. (FR-CHR-004)
- **FR-008**: The timer state (mode, set time, phase, running or paused, target time or remaining time, completed
  cycles) MUST survive eject, restarts and crashes. If the target time passed while the watch was off, the alert MUST
  be raised at boot and marked late.
- **FR-009**: Clock changes (time zone, synchronisation corrections) MUST NOT make the remaining time jump by more
  than the size of the correction; time zone changes MUST have no effect.

**Pomodoro**
- **FR-010**: A Pomodoro mode MUST alternate work (default 25 min) and break (default 5 min) intervals automatically,
  show the current phase and the count of completed work intervals, and alert at each transition: BINGO FUEL
  (vibration + voice) at the end of work, BREAK OVER (vibration + chime) at the end of a break. (FR-CHR-005)
- **FR-011**: The work and break lengths MUST be adjustable (work: the countdown choices; break: 5, 10 or 15
  minutes) and kept across restarts.

**Page and access**
- **FR-012**: A CHRONO page MUST be added to the page ring (PFD → WX → RECORDER → CHRONO → SETTINGS). Its controls
  MUST be at least the standard touch-target size, and it MUST follow NVG mode.
- **FR-013**: The diagnostic console MUST show the timer state and offer test hooks: start with a time in seconds,
  pause, resume, reset, Pomodoro with test interval lengths in seconds.

**Quality**
- **FR-014**: The timer state machine (start, pause, resume, reset, expiry, Pomodoro transitions, late expiry,
  resume after restart) MUST be tested off-device. (NFR-MNT-004)
- **FR-015**: The timer MUST NOT reduce the watch face's smoothness below the accepted level (median ≥30 fps), and
  an idle timer MUST NOT add power drain.

### Key Entities

- **Timer settings**: mode (countdown or Pomodoro), countdown length (preset or custom), Pomodoro work and break
  lengths; persisted.
- **Timer state**: idle / running / paused / expired; current phase (countdown, work, break); interval length; target
  end time (when running) or remaining time (when paused); completed work intervals; late flag.
- **Alerts used**: BINGO FUEL (caution, existing S4/S5 id and voice clip) and BREAK OVER (caution, new, chime).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A 1-minute timer fires within ±1 s of its target time in 3 of 3 runs in each case: staying on the timer
  page, on another page, with the screen off, and ejected.
- **SC-002**: Pause/resume keeps the remaining time exact (difference ≤ 1 s) in 5 of 5 trials with pauses of 5–60 s.
- **SC-003**: A Pomodoro session with test intervals completes work → break → work → break with an alert at every
  transition and a correct cycle count, including one transition that happens while ejected.
- **SC-004**: After a restart during a running timer, the remaining time is within 1 s of what it should be; a
  timer that expired while off is reported late at boot.
- **SC-005**: The wearer can set, start, pause and reset a timer in under 10 seconds and confirms that the tank and
  MM:SS are readable without studying the screen (wearer check, answer recorded verbatim, also in NVG).
- **SC-006**: The watch face keeps a median ≥30 fps while a timer runs.

## Assumptions

- **Time base**: the timer is based on the watch's own real-time clock, which keeps running in deep sleep and across
  restarts. An unset clock still counts correctly; only large clock corrections can shift the target (bounded by
  the correction, FR-009).
- **Pomodoro**: there is no long break after every 4th interval (kept simple); the session runs until reset. The
  cycle count resets with the session.
- **Alerts**: BINGO FUEL uses the existing caution, vibration pattern and voice clip from S4/S5. BREAK OVER is a new
  caution using the cabin chime. Both follow quiet mode and quiet hours as other cautions do.
- **One timer**: only one timer (countdown or Pomodoro) runs at a time. A stopwatch is out of scope.
- **Page order**: CHRONO sits between RECORDER and SETTINGS.
- **Waking the watch**: the timer wake from deep sleep reuses S7's eject sequence (the pre-sleep step reports the
  target time). Wake by wrist motion stays unavailable (ADR-014).
- **T-Watch S3** must compile; deep sleep is not validated there (S7), so the timer simply keeps running awake there.
- **Out of scope**: a stopwatch or lap timer, several timers, alarms at a clock time, complications on the watch face.

## Dependencies

- S4 (merged): alert list, cautions, vibration patterns, acknowledge.
- S5 (merged): "Bingo Fuel" voice clip and chime, quiet mode and quiet hours.
- S7 (merged): screen timeout with alert wake, Pull-to-Eject with the pre-sleep hook and timer wake, NVG.
- S2 (merged): synchronised clock (corrections must not disturb the timer).
- S0 (merged): settings storage, page ring, console.
