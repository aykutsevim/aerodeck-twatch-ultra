# Feature Specification: G-Force & Flight Recorder (Slice S6)

**Feature Branch**: `007-flight-recorder`

**Created**: 2026-11-12

**Status**: Draft

**Input**: User description: "S6 on new feat branch"

**Traceability**: AD-SRS-001 FR-REC-001…005, FR-PFD-008 · Roadmap milestone M2 (last slice) · Depends on S1 (PFD
watch face with reserved MAX G / FLT NM slots, motion sensor), S2 (local time and time zone) · Prompt source:
`docs/speckit_slice_prompts.md` § S6

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Flight Miles from my steps (Priority: P1)

As the wearer, I want my walking turned into "Flight Miles": today's steps converted to nautical miles using my
stride length (default 0.75 m; 1 NM = 1852 m). Today's distance shows on the watch face in the FLT NM slot, so a
glance tells me how far I have "flown" today.

**Why this priority**: It is the slice's headline value and fills a slot that has shown "--" since S1.

**Independent Test**: Walk a measured distance with the stride set to the wearer's real stride, and compare the
FLT NM value before and after with the true distance.

**Acceptance Scenarios**:

1. **Given** the default stride (0.75 m), **When** 2 470 steps are counted, **Then** FLT NM shows 1.00 (2 470 ×
   0.75 m = 1 852.5 m).
2. **Given** I walk a measured 1 000 m with my stride set correctly, **When** I look at FLT NM, **Then** it has
   increased by 0.54 NM ± 15 %.
3. **Given** I change the stride in Settings, **When** I look at the watch face, **Then** today's NM is recomputed
   from today's steps with the new stride (steps are the source of truth).
4. **Given** the watch has just started and no steps were counted today, **When** I look at the face, **Then** FLT NM
   shows 0.00 (not "--").
5. **Given** the step counter is unavailable (motion sensor failed), **When** I look at the face, **Then** FLT NM
   shows "--" and the rest of the watch works.

---

### User Story 2 - Max G with the time it happened (Priority: P1)

As the wearer, I want the watch to record today's highest g-force from sudden wrist movements ("Max G"), with the
time it happened. The watch face shows it in the MAX G slot (e.g. "2.7").

**Why this priority**: It is the second headline value of the slice and completes the PFD (FR-PFD-008).

**Independent Test**: Hold the wrist still (MAX G ≈ 1.0), then make a sharp wrist flick, and check that MAX G
rises and the recorder shows the flick's time.

**Acceptance Scenarios**:

1. **Given** a new day with only gentle movement, **When** I look at MAX G, **Then** it shows about 1.0–1.5.
2. **Given** I make a sharp wrist flick, **When** I look at MAX G within 1 second, **Then** it has risen above 2.0
   and the recorder page shows the local time of the flick (HH:MM).
3. **Given** a lower peak happens later, **When** I look again, **Then** MAX G and its time are unchanged (it only
   rises during a day).
4. **Given** the motion sensor is unavailable, **When** I look at the face, **Then** MAX G shows "--".

---

### User Story 3 - Today's values survive restarts (Priority: P1)

As the wearer, I don't want to lose today's miles and Max G when the watch restarts (update, crash, or the battery
running flat). The values must still be there after a restart.

**Why this priority**: A recorder that forgets on every restart is useless; FR-REC-005.

**Independent Test**: Walk some steps and flick the wrist, restart the watch from the console, and check that the
same NM, steps and Max G (with its time) are shown afterwards.

**Acceptance Scenarios**:

1. **Given** today's values, **When** the watch restarts in a planned way (console restart), **Then** today's
   steps, NM, Max G and its time are exactly the same afterwards, and counting continues from there.
2. **Given** today's values, **When** power is cut abruptly (or the watch is reset by flashing new firmware over USB,
   which gives no warning), **Then** after the restart at most the last hour of steps and Max G is lost.
3. **Given** I use "Restore defaults" in Settings, **When** the settings return to their defaults, **Then** today's
   values and the history are kept (only the stride returns to 0.75 m).
4. **Given** the 7-day history, **When** the watch restarts, **Then** all stored days are still there.
5. **Given** the motion sensor's own step count starts again from zero after a restart, **When** I keep walking,
   **Then** today's steps continue from the saved value (no drop to zero, no double counting).

---

### User Story 4 - Recorder page with a 7-day chart (Priority: P2)

As the wearer, I want a Recorder page in the page ring that shows today's nautical miles, steps, Max G with its
time, and a bar chart of the last 7 days, so I can see my week at a glance.

**Why this priority**: The watch face already shows today's two key numbers; the page adds detail and history.

**Independent Test**: Swipe to the Recorder page and check today's values against the console; with history
present, check that the chart shows one bar per day, scaled to the largest day, with today highlighted.

**Acceptance Scenarios**:

1. **Given** today's data, **When** I open the Recorder page, **Then** it shows today's NM, steps, Max G and the
   time of Max G, matching the watch face.
2. **Given** history for previous days, **When** I look at the chart, **Then** it shows 7 bars (the previous 6 days
   and today, today highlighted), each labelled with its weekday, heights proportional to NM, and days without data
   shown as empty bars.
3. **Given** I tap a bar, **When** it is selected, **Then** that day's NM, steps and Max G are shown.
4. **Given** I swipe through the pages, **When** I reach the Recorder page, **Then** it sits where the "PAGE B"
   placeholder was (PFD → WX → RECORDER → SETTINGS).

---

### User Story 5 - New day at local midnight (Priority: P2)

As the wearer, my counters start again at local midnight, and yesterday moves into the history, so each day is
recorded separately. The last 7 days are kept.

**Why this priority**: Correct daily boundaries make the history meaningful; FR-REC-003.

**Independent Test**: Set the clock to 23:59:50 local, let midnight pass, and check that today's counters are zero
and yesterday's values appear as the newest history bar.

**Acceptance Scenarios**:

1. **Given** it is 23:59:59 local, **When** the clock passes midnight, **Then** today's steps, NM and Max G reset
   within 2 seconds, and the finished day is stored in the history with its steps and Max G.
2. **Given** 7 previous days are stored, **When** a new day starts, **Then** the oldest day is dropped.
3. **Given** the watch was off over one or more midnights, **When** it starts on a later day, **Then** the saved day
   goes into the history under its own date, the skipped days appear empty, and today starts at zero.
4. **Given** the clock is corrected backwards (sync or time zone change) so the local date goes back, **When** that
   happens, **Then** no extra day boundary is created and today's values keep counting.

---

### Edge Cases

- **Clock not set** (no valid time): values are counted into "today"; no midnight rollover happens until the time is
  valid. When the clock becomes valid, the counted values belong to the current local date.
- **Time zone change** during the day: the day boundary follows the new local time; at most one rollover per local
  date; never a reset caused only by the change itself unless the local date moved forward.
- **Motion sensor missing or failed:** FLT NM and MAX G show "--"; the Recorder page says the sensor is
  unavailable; history stays visible.
- **Implausible step jumps** (e.g. the sensor reports a sudden huge count after a reset): a single jump larger than
  what a person can walk between two readings is ignored and logged.
- **Max G sensor limit:** very hard knocks may exceed the sensor's measuring range; the value is shown as the range
  limit followed by "+" (e.g. "16.0+"), not as an error.
- **Charging / at rest:** no special handling; at rest Max G stays near 1.0.
- **Stride out of range:** the stride setting only accepts 0.30–1.50 m.
- **Flash wear:** saving at most about once per hour plus at midnight and before a planned restart keeps writes low.
- **Display smoothness:** recording must not reduce the watch face's smoothness (see SC-006).

## Requirements *(mandatory)*

### Functional Requirements

**Flight Miles**
- **FR-001**: The watch MUST count today's steps from the motion sensor and convert them to nautical miles:
  NM = steps × stride (m) ÷ 1852. (FR-REC-001)
- **FR-002**: The stride length MUST be settable on the watch and over the diagnostic console, 0.30–1.50 m
  (default 0.75 m), and kept across restarts. Changing it MUST recompute today's and the history's NM from the
  stored step counts.
- **FR-003**: The watch face MUST show today's NM in the FLT NM slot: two decimals below 10 NM, one decimal from
  10 NM, "--" when the step counter is unavailable. (FR-PFD-008)

**Max G**
- **FR-004**: The watch MUST track the peak acceleration magnitude including gravity, in g, sampled at least 50
  times per second, and keep today's Max G and the local time it occurred. (FR-REC-002)
- **FR-005**: The watch face MUST show today's Max G in the MAX G slot with one decimal, updated within 1 s of a new
  peak, "--" when the motion sensor is unavailable. (FR-PFD-008)

**Days and history**
- **FR-006**: Today's counters MUST reset at local midnight (within 2 s), and the finished day MUST be stored with
  its date, steps and Max G (with time). The previous 7 days MUST be kept; older days are dropped. (FR-REC-003)
- **FR-007**: The day boundary MUST use the local date from the watch's clock and time zone; there MUST be no
  rollover while the clock is not set, at most one rollover per local date, and none when the local date moves
  backwards.
- **FR-008**: After the watch was off across midnights, the saved day MUST go into the history under its own date
  and skipped days MUST appear empty.

**Persistence**
- **FR-009**: Today's values and the history MUST survive restarts: saved at least once per hour, at each rollover,
  and before every planned restart; after an abrupt power loss at most one hour of data may be lost. A save on
  demand MUST be available for later slices (deep sleep, S7). (FR-REC-005)
- **FR-010**: Today's step count MUST continue correctly when the sensor's own counter restarts from zero (after a
  restart or sensor reset): no loss beyond FR-009, no double counting.

**Recorder page**
- **FR-011**: A Recorder page MUST replace the "PAGE B" placeholder in the page ring and show today's NM, steps,
  Max G and its time, and a 7-bar chart (previous 6 days + today, today highlighted, weekday labels, NM heights
  scaled to the largest bar, empty bars for days without data). Tapping a bar MUST show that day's NM, steps and
  Max G. The 7th previous day is kept for the console and later use but is not drawn. (FR-REC-004)

**Diagnostics and quality**
- **FR-012**: The diagnostic console MUST show the recorder state (today, history, sensor status, last save) and
  offer test hooks to add simulated steps, inject a g value, force a save, and simulate a day rollover.
- **FR-013**: The NM conversion, the peak tracking, the day-boundary rules (incl. clock not set, time zone change,
  backward clock, skipped days), the step-counter restart handling and the history ring MUST be tested
  off-device. (NFR-MNT-004)
- **FR-014**: Recording MUST NOT reduce the watch face's smoothness below the level accepted in S1–S5 (median
  ≥28 frames per second).
- **FR-015**: Restoring the settings defaults MUST NOT erase the recorder data (it is user data, not a setting).

### Key Entities

- **Day record**: local date, steps, Max G (g) and the local time of Max G. NM is derived from steps and the stride.
- **Today**: the day record being counted, plus the last step-counter reading used to add new steps.
- **History**: the previous 7 day records, newest first.
- **Stride setting**: length in metres (0.30–1.50, default 0.75), persisted with the other settings.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Walking a measured distance of at least 500 m with the stride set to the wearer's stride gives FLT NM
  within ±15 % of the true distance, in 3 of 3 walks.
- **SC-002**: In 10 of 10 sharp wrist flicks, MAX G rises above 2.0 within 1 second and the recorded time matches
  the flick's minute.
- **SC-003**: After 10 of 10 planned restarts, today's steps, Max G and history are unchanged; after an abrupt
  power cut, no more than one hour of data is lost.
- **SC-004**: In 3 of 3 simulated midnights (clock set to 23:59:50), the counters reset within 2 s and the finished
  day appears as the newest history bar; a backward clock change and a time zone change cause no extra rollover.
- **SC-005**: The Recorder page shows the same today values as the watch face and the console in 5 of 5 checks,
  and the wearer finds the 7-day chart readable.
- **SC-006**: While recording, the watch face keeps a median of ≥28 frames per second, as accepted in earlier
  slices.

## Assumptions

- **Steps come from the motion sensor's own step counter** (a built-in function of the T-Watch Ultra's sensor hub). Its accuracy sets the NM accuracy; S6 does no step detection of its own.
- **Max G** uses the same motion sensor's acceleration including gravity; at rest it reads about 1.0 g. The sensor's
  measuring range limits the highest value shown.
- **T-Watch S3** must compile; recorder features that depend on the Ultra's sensor hub may be unavailable there
  ("--"), per constitution VII.
- **Deep sleep** arrives in S7 (Pull-to-Eject). S6 provides the save-on-demand that S7 calls before sleeping;
  "survives deep sleep" (FR-REC-005) is verified in S7. S6 verifies restarts and power loss.
- **"Battery swaps"** in the slice prompt means the battery being drained or disconnected: this watch's battery is
  not user-replaceable, so it is covered by the abrupt power-loss case.
- **Flight Miles** are walking distance in nautical miles; running uses the same stride (no pace-dependent stride).
- **Weekday labels** use the watch's local date; the chart scale is per view (largest of the 7 bars = full height).
- **Display smoothness target** follows the S2–S5 accepted level (median ≥28 fps), measured as in earlier slices.
- **Out of scope:** GPS distance, calories, heart rate, goals/notifications, export of history, more than 7 days.

## Dependencies

- S1 (merged): PFD with reserved MAX G / FLT NM slots; motion sensor running.
- S2 (merged): local time with named time zones and clock validity (day boundary).
- S0 (merged): settings storage, page ring, console, peripheral status.
- S7 (later): deep sleep will call the recorder's save-on-demand.
