# Feature Specification: GPWS Audio, Voice Callouts & Cabin Chime (Slice S5)

**Feature Branch**: `006-gpws-audio`

**Created**: 2026-11-11

**Status**: Draft

**Input**: User description: "S5 on new feat branch"

**Traceability**: AD-SRS-001 FR-AUD-001…005, NFR-PWR-003 · Roadmap milestone M2 · Depends on S4 (merged: alert table
with a voice-clip slot per alert, quiet mode) · Prompt source: `docs/speckit_slice_prompts.md` § S5

## User Scenarios & Testing *(mandatory)*

### User Story 1 - The watch speaks its warnings (Priority: P1)

As the wearer, when something important happens, the watch says it out loud, like a cockpit warning system:
- **"Pull up"** when the battery is critically low;
- **"Stall"** when the battery goes below 15 %;
- **"Master caution"** when a new caution appears.

A warning keeps repeating its callout until I acknowledge it, together with the vibration from S4. Acknowledging
silences it immediately.

**Why this priority**: This is the slice's core value, and it makes the S4 alerts unmistakable even when the watch isn't in
view.

**Independent Test**: Raise STALL, PULL UP and a CAUTION from the diagnostic console. Each speaks its callout once on
raise; the warnings repeat until acknowledged; tapping the band stops them.

**Acceptance Scenarios**:

1. **Given** sound is on and quiet mode is off, **When** STALL is raised, **Then** "Stall" is heard within 1 second,
   together with the stick shaker.
2. **Given** STALL is active and not acknowledged, **When** time passes, **Then** "Stall" repeats about every
   6 seconds until acknowledged.
3. **Given** a warning is repeating, **When** I tap the band, **Then** speech stops within 0.5 seconds (a clip in
   progress is cut off) and does not repeat.
4. **Given** the battery reaches 5 %, **When** PULL UP is raised, **Then** "Pull up" is heard, and it repeats until
   acknowledged.
5. **Given** a CAUTION is raised (e.g. a peripheral failure), **When** it appears, **Then** "Master caution" is heard
   once. Cautions don't repeat.
6. **Given** an ADVISORY is raised (e.g. TIME UNSYNC), **When** it appears, **Then** no voice is played (the band and
   the gentle vibration are enough).
7. **Given** a callout is playing, **When** a more important alert arrives (a warning during "Master caution"),
   **Then** the current clip stops and the warning's callout plays at once.

---

### User Story 2 - Silent when nothing plays (Priority: P1)

As the wearer, sound must cost no battery when the watch is silent: the speaker amplifier is switched on only while a
sound is playing, and off again right after.

**Why this priority**: Battery life is a core requirement (NFR-PWR-003, constitution III). An amplifier left on would
drain the watch.

**Independent Test**: Watch the power log while playing clips. The amplifier switches on before each sound and off
within half a second after the last one. It stays off while idle.

**Acceptance Scenarios**:

1. **Given** nothing is playing, **When** I check the power state, **Then** the amplifier is off.
2. **Given** a clip is requested, **When** it starts, **Then** the amplifier is on, and the start of the sound is not
   cut off.
3. **Given** several clips play back-to-back, **When** the last one ends, **Then** the amplifier switches off within
   0.5 seconds.
4. **Given** the amplifier is switched on and off, **When** it happens, **Then** there is no loud pop or click from
   the speaker.

---

### User Story 3 - Hourly cabin chime (Priority: P2)

As the wearer, a soft two-tone cabin chime plays at the top of every hour, like the seat-belt chime in an airliner,
so I keep track of time without looking. It stays silent at night (quiet hours, default 22:00–07:00 local time) and
when quiet mode is on.

**Why this priority**: It is a signature comfort feature, but not safety-relevant.

**Independent Test**: Set the clock to just before a full hour outside quiet hours and check that the chime plays at
hh:00. Repeat inside quiet hours and with quiet mode on: no chime.

**Acceptance Scenarios**:

1. **Given** the local time reaches hh:00:00 outside quiet hours, **When** the hour turns, **Then** the chime plays
   within 2 seconds, once.
2. **Given** the local time is 23:00 with default quiet hours, **When** the hour turns, **Then** no chime plays.
3. **Given** quiet mode is on, **When** the hour turns, **Then** no chime plays.
4. **Given** the hourly chime is switched off in Settings, **When** the hour turns, **Then** no chime plays.
5. **Given** the clock is not set (time unknown), **When** time passes, **Then** no chime plays.
6. **Given** the clock is changed across an hour boundary (time sync, time zone change), **When** that happens,
   **Then** at most one chime plays for that hour, and none for hours skipped over.

---

### User Story 4 - Volume and sound settings (Priority: P2)

As the wearer, I set the volume in Settings (Off, 1, 2, 3, 4), switch the hourly chime on or off, and set the quiet
hours. I can play a sample sound to hear the volume. The settings are kept across reboots.

**Why this priority**: Comfort and social acceptability; the defaults already work.

**Independent Test**: Change the volume and play the sample at each step. Set the volume to Off and check that nothing
plays at all. Change the quiet hours and check the chime follows them. Reboot and check
that the settings are kept.

**Acceptance Scenarios**:

1. **Given** volume steps 1–4, **When** the sample plays at each, **Then** each step is clearly louder than the one
   before.
2. **Given** volume is Off, **When** any sound would play, **Then** nothing plays and the amplifier stays off.
3. **Given** I set quiet hours 23:00–06:00, **When** the hour turns at 22:00, **Then** the chime plays. **When** it
   turns at 23:00, **Then** it doesn't.
4. **Given** I changed the settings, **When** the watch reboots, **Then** they are kept.

---

### User Story 5 - A library of cockpit callouts for later features (Priority: P3)

As the wearer and as the developer of later slices, the watch carries the full set of callouts now:
- "Master caution";
- "Master warning";
- "Stall";
- "Pull up";
- "Bingo fuel" (timer finished, S8);
- "Minimums" (generic alarm);
- "Traffic" (radar, S9);
- the cabin chime.

All of them are original recordings (generated speech), not copied from aircraft or films. Each can be played from
the diagnostic console.

**Why this priority**: Later slices only need to refer to a clip. It also lets the wearer hear them all now.

**Independent Test**: Play each clip from the diagnostic console and check that it is clear, complete, and of similar
loudness.

**Acceptance Scenarios**:

1. **Given** the clip library, **When** I play each clip, **Then** each is clearly understandable, without clipping
   or noise, and they are of similar loudness.
2. **Given** the project files, **When** someone rebuilds the clips, **Then** a documented, repeatable procedure
   produces them from text (no third-party recordings).
3. **Given** a clip file is missing or damaged on the watch, **When** it would play, **Then** nothing plays, a
   diagnostic line names the clip, and the watch keeps working (the vibration and band still alert).

---

### Edge Cases

- **Speaker or amplifier missing/failed:** alerts still vibrate and show; a SPEAKER FAIL caution exists from S0. No
  retries in a tight loop.
- **Audio files not installed** (e.g. firmware flashed without the files): the watch works silently and logs once per
  missing clip. The firmware build does not depend on the files being present.
- **Many requests at once** (several alerts, a chime): sounds queue in priority order (warning > caution > chime/sample).
  The queue is bounded; low-priority sounds are dropped rather than delayed for long (a chime that can't start within
  2 seconds of its request is skipped, so it is never heard late).
- **The same warning raised again while its callout is queued:** no duplicate in the queue.
- **Quiet mode on:** no sound at all, warnings included. Warnings still vibrate (S4 rule, FR-ALR-004).
- **Quiet hours:** silence the chime and caution callouts. Warning callouts still play (a dying watch at night still
  says "Pull up"). Quiet hours wrap past midnight (22:00–07:00).
- **Volume Off:** silences everything, like quiet mode for sound.
- **Low battery:** sounds still play down to the PULL UP level. Audio adds no special battery logic beyond the
  amplifier rule.
- **Display smoothness:** playback must not make the watch face stutter (see SC-005).
- **Page changes during playback:** the sound continues; it is not tied to a page.

## Requirements *(mandatory)*

### Functional Requirements

**Playback**
- **FR-001**: The watch MUST play short sound clips stored on its own file storage through its speaker. (FR-AUD-001)
- **FR-002**: Sounds MUST be played one at a time from a bounded queue in priority order:
  warning callout > caution callout > chime > sample/test.

  A higher-priority sound MUST interrupt a lower-priority one that is playing. A sound that cannot start within 5 s of
  its request MUST be dropped (the hourly chime: within 2 s, FR-009).
- **FR-003**: The speaker amplifier MUST be powered only while sound is playing, and switched off within 0.5 s after
  the queue empties. (FR-AUD-004, NFR-PWR-003)
- **FR-004**: Starting and stopping the amplifier MUST NOT produce an audible pop, and the beginning of a clip MUST NOT
  be cut off.

**Callouts**
- **FR-005**: Each alert type MAY name a voice callout (the S4 alert table's voice slot). In S5:
  - STALL → "Stall";
  - PULL UP → "Pull up";
  - every CAUTION → "Master caution";
  - ADVISORY → none, for every alert (an alert's own callout is used only at CAUTION or WARNING level);
  - the S4 test alerts → "Minimums".

  (FR-AUD-003)
- **FR-006**: A callout MUST start within 1 s of its alert being raised (or re-armed). WARNING callouts MUST repeat
  about every 6 s while the warning is unacknowledged. CAUTION callouts MUST play once per raise.
- **FR-007**: Acknowledging alerts MUST stop any warning or caution callout in progress within 0.5 s, and cancel its
  queued repeats.
- **FR-008**: The clip library MUST include "Master caution", "Master warning", "Stall", "Pull up", "Bingo fuel",
  "Minimums", "Traffic" and a two-tone cabin chime. All of them MUST be original or text-to-speech, and reproducible
  from a documented procedure in the project. (FR-AUD-003, constitution VI)

**Chime**
- **FR-009**: When the hourly chime is enabled (default on), a chime MUST play once at each full local hour (hh:00,
  within 2 s), unless quiet mode is on, the volume is Off, the time is in the quiet hours (default 22:00–07:00,
  inclusive start, exclusive end, wrapping past midnight), or the clock is not set. (FR-AUD-002)
- **FR-010**: Clock changes (sync, time zone) MUST NOT cause more than one chime per hour, nor chimes for skipped hours.

**Settings and rules**
- **FR-011**: Volume MUST be selectable in five steps: Off, 1, 2, 3, 4 (default 3; changed from 2 after the wearer found step 2 too quiet). Each step MUST be clearly louder
  than the one before. The setting MUST be kept across reboots. (FR-AUD-005)
- **FR-012**: Quiet mode (from S4) MUST suppress all sound. Quiet hours MUST suppress the chime and caution callouts,
  but not warning callouts. Volume Off MUST suppress all sound. (FR-ALR-004)
- **FR-013**: Settings MUST offer, on the watch: Volume (Off/1–4), Hourly chime (on/off), quiet hours start and end
  (whole hours), and a "Play sample" control. All of them MUST be settable over the diagnostic console as well.
- **FR-014**: The diagnostic console MUST offer commands to play any clip by name, stop playback, show the audio state
  (amplifier on/off, playing clip, queue length), and trigger the chime decision for a given time (test hook).

**Robustness and quality**
- **FR-015**: A missing or unreadable clip MUST be skipped with one diagnostic line, never crashing or blocking the
  watch. The firmware MUST run (silently) without the audio files.
- **FR-016**: The chime decision (hour boundary, quiet-hours window incl. wrap, quiet mode, volume, clock validity, no
  duplicates) and the queue priority rules MUST be tested off-device. (NFR-MNT-004)
- **FR-017**: Playback MUST NOT reduce the watch face's smoothness below the level accepted in S4 (median ≥28 frames
  per second with sound playing).

### Key Entities

- **Clip**: a named, short recorded sound (callout or chime) stored on the watch, with a priority class.
- **Sound request**: a clip, its priority and the time it was requested, waiting in the queue.
- **Audio settings**: volume (Off/1–4), hourly chime on/off, quiet hours start/end. Quiet mode comes from S4. All are
  persisted.
- **Alert → callout mapping**: the S4 alert table's voice slot, now filled.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In 10 of 10 raises each, STALL, PULL UP and a CAUTION are heard within 1 s with the right callout, and an
  acknowledged warning goes silent within 0.5 s.
- **SC-002**: With no sound playing, the amplifier is off 100 % of the time over a 1-hour idle log. After any
  playback, it is off within 0.5 s, in 20 of 20 cases.
- **SC-003**: The hourly chime plays at hh:00 (±2 s) in 5 of 5 tested hours outside quiet hours, and 0 of 5 inside
  quiet hours or with quiet mode on.
- **SC-004**: The wearer judges all 8 clips clear and of similar loudness, and each volume step audibly louder than the
  previous.
- **SC-005**: While sound plays, the watch face keeps a median of ≥28 frames per second (as accepted in S4), and the
  user sees no stutter.
- **SC-006**: With the audio files removed, the watch boots and works normally, and alerts still vibrate.

## Assumptions

- **Hardware:** both supported watches have a small speaker with a digital amplifier on a switchable power rail
  (`AudioAmp`), already modelled in S0. The speaker is small, so callouts are designed for clarity, not bass.
- **Storage:** the clips live in the watch's own file storage, installed by a separate upload step from the firmware
  (ADR-009 governs which partition). Total size is small (a few hundred kB).
- **Voices** are generated with a text-to-speech engine available on the development PC. The exact engine and voice
  are recorded in the procedure (FR-008). The chime is synthesized (two sine tones with decay), not recorded.
- **Callout mapping** in S5:
  - "Master warning" is reserved for future WARNING types without their own callout;
  - "Bingo fuel" and "Traffic" are used by S8 and S9;
  - "Minimums" is the generic alarm, used now by the test alert.
- **Repeat interval** for warning callouts is about 6 s (every third stick-shaker cycle), so speech doesn't run
  continuously.
- **Quiet hours** apply to the chime and caution callouts only. Warnings stay audible unless quiet mode or volume Off.
- **Out of scope:** microphone use, voice recognition, music, per-alert volume, recording.
- **Display smoothness target:** follows the S2–S4 accepted level (median ≥28 fps), measured as in earlier slices.

## Dependencies

- S0 (merged): power rails (`AudioAmp`), file storage partition, peripheral status (SPEAKER), settings, console.
- S4 (merged): alert table voice slot, AlertManager raise/re-arm/acknowledge events, quiet mode, warning repeat timing.
- S1/S2 (merged): local time (time zone) for the hourly chime and quiet hours; clock validity.
- S8 (timer, "Bingo fuel") and S9 (radar, "Traffic") will reference clips added here.
