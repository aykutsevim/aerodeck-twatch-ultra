# Feature Specification: PFD Watch Face (Slice S1)

**Feature Branch**: `002-pfd-watch-face`

**Created**: 2026-11-11

**Status**: Draft

**Input**: User description: "S1 PFD watch face"

**Traceability**: AD-SRS-001 FR-PFD-001…009, **FR-PFD-011** (new: degraded attitude flags), FR-TIM-003 (amended:
interim fixed offset), **FR-TIM-006** (new: clock validity / CLOCK NOT SET), **FR-TIM-007** (new: diagnostic clock set
until S2), NFR-PERF-001, NFR-PERF-002, NFR-USE-002 · Roadmap milestone M1 ·
Depends on S0 (platform foundation, merged) · Prompt source: `docs/speckit_slice_prompts.md` § S1

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Artificial horizon that follows my wrist (Priority: P1)

As the wearer, my home screen is an aircraft Primary Flight Display. In the middle sits an artificial horizon:
blue sky above, brown ground below, with a fixed aircraft symbol (crosshair) at the centre. When I roll my
wrist, the horizon line rotates the opposite way, as a real attitude indicator does. When I tilt my wrist
forward or back, the horizon moves up or down along a pitch scale. When I hold my wrist level, a green
**LOCK** indicator lights at the crosshair.

**Why this priority**: The moving horizon is the signature of the product. It is also the most demanding
part technically (smooth motion), so it must be proven first.

**Independent Test**: Flash the firmware, hold the watch in front of you, and roll and pitch it. Watch the
horizon track the motion, then lay the watch flat and see LOCK appear.

**Acceptance Scenarios**:

1. **Given** the watch lies flat, face up, **When** the PFD is shown, **Then** the horizon line is
   horizontal through the centre, and LOCK lights after about half a second.
2. **Given** the watch is level, **When** I roll it 30° to the right, **Then** the horizon rotates 30° the
   other way (bank shown as in an aircraft), and LOCK turns off.
3. **Given** the watch is level, **When** I pitch it 10° nose-up, **Then** the horizon moves down by the
   amount the pitch scale shows for 10°.
4. **Given** LOCK is lit, **When** I tip the wrist slightly (less than 5°), **Then** LOCK stays lit (no
   flicker). **When** I tip more than 5°, **Then** LOCK turns off.
5. **Given** any wrist motion, **When** I watch the screen, **Then** the horizon moves smoothly with no
   visible stutter and no noticeable lag behind my wrist.

---

### User Story 2 - Read the time like flight instruments (Priority: P1)

As the wearer, I read the time from the instrument tapes:
- The left vertical "airspeed" tape shows the **hour (00–23)**.
- The right vertical "altitude" tape shows the **minute (00–59)**.
- The top "heading" tape scrolls smoothly with the **seconds**, one second equal to 6° of heading (0 to 354).

Each tape highlights the current value in a boxed readout. The bottom corners show **ZULU** (UTC) and
**LOCAL** time as HH:MM in a high-contrast green HUD style.

**Why this priority**: It's a watch. Telling the time is the primary job, and it must work at a glance.

**Independent Test**: Compare the tapes and corner readouts with a reference clock over several minutes,
including across a minute and an hour change.

**Acceptance Scenarios**:

1. **Given** the time is 14:37:20 local, **When** I look at the PFD, **Then** the hour tape box reads 14,
   the minute tape box reads 37, and the heading box reads 120.
2. **Given** a second passes, **When** I watch the heading tape, **Then** it scrolls continuously by 6°,
   without jumping.
3. **Given** the time is 23:59:59 local, **When** the next second arrives, **Then** the tapes read 00, 00
   and 000, and the ZULU/LOCAL readouts update correctly (including the date rollover of UTC vs local).
4. **Given** the clock has never been set, **When** the PFD is shown, **Then** the readouts show dashes
   ("--") and a caution says the clock is not set. No invented time is displayed.
5. **Given** I'm reading the watch in daylight, **When** I glance at it, **Then** hour, minute and the
   corner times are legible at arm's length.

---

### User Story 3 - Battery as engine gauges (Priority: P2)

As the wearer, a strip along the bottom of the PFD (the "EICAS") turns the battery into engine gauges:
- **FUEL QTY**: battery charge in percent.
- **FUEL FLOW**: how fast the battery is draining, as percent per hour.
- **EGT**: the power chip's temperature in °C.

While the watch is charging, the strip shows a charging annunciator, and FUEL FLOW shows as a refuel. When
the battery is low, a low-fuel caution appears.

**Why this priority**: It adds character and useful battery information. The watch is still usable as a
clock without it.

**Independent Test**: Read the gauges on battery and while charging. Compare FUEL QTY and EGT with the
diagnostic readout, and check that FUEL FLOW settles to a plausible drain rate.

**Acceptance Scenarios**:

1. **Given** the watch runs on battery, **When** I look at the EICAS, **Then** FUEL QTY matches the battery
   percent from the diagnostic readout within 1 point, and updates within 2 seconds of a change.
2. **Given** the watch has been on battery for at least 10 minutes, **When** I read FUEL FLOW, **Then** it
   shows a drain rate in %/h. Before enough history exists, it shows "--".
3. **Given** I plug in USB power, **When** charging starts, **Then** a charging annunciator appears within
   2 seconds, and FUEL FLOW shows a refuel indication instead of a drain rate.
4. **Given** the battery drops below 15 % and is not charging, **When** I look at the PFD, **Then** a
   "LOW FUEL" caution is shown. It clears when charging starts or the battery rises above 17 %.

---

### User Story 4 - Reserved recorder readouts (Priority: P3)

As the wearer, I see two small readouts reserved for later features: **MAX G** and **FLIGHT NM**. In this
slice they show "--" until the flight recorder (slice S6) fills them.

**Why this priority**: Layout only. Reserving the space now avoids redesigning the face later.

**Independent Test**: Both readouts are visible on the PFD showing "--".

**Acceptance Scenarios**:

1. **Given** the PFD is shown, **When** I look for MAX G and FLIGHT NM, **Then** both are visible and read "--".

### Edge Cases

- **Watch upside down or vertical** (pitch beyond ±80°): the horizon shows full sky or full ground at the
  scale limit instead of spinning or jumping erratically, and LOCK is off.
- **Motion sensor unavailable** (failed or simulated failure): the horizon shows a fixed level picture with
  a clear "ATT FAIL" flag, LOCK stays off, and timekeeping and the EICAS still work.
- **Clock not set or invalid**: readouts show "--", with a caution (see US2 #4).
- **Power chip reading temporarily unavailable**: the EICAS values show "--" instead of stale or zero values.
- **Pitch scale near limits**: pitch markings stay readable, and the horizon never leaves the instrument
  area.
- **Fast wrist flick**: the horizon may briefly lag, but it settles to the true attitude within half a
  second, without overshoot oscillation.
- **Navigating away and back**: leaving the PFD stops its work (no drawing in the background). Returning
  shows current values immediately.
- **Overlays**: the caution banner and the page indicator from S0 still appear above the PFD, and don't
  cover the hour/minute boxes for longer than their normal display time.

## Requirements *(mandatory)*

### Functional Requirements

**Attitude (horizon)**

- **FR-001**: The home page MUST be a Primary Flight Display with a central attitude indicator: sky above,
  ground below, a fixed aircraft symbol at the centre, and a pitch scale (SRS FR-PFD-001).
- **FR-002**: The horizon MUST rotate opposite to wrist roll and shift with wrist pitch, using the watch's
  motion sensing. A pitch scale MUST make the pitch angle readable (marks at least every 10°).
- **FR-003**: A green LOCK indicator MUST light when roll and pitch are both within ±3° for at least 0.5 s,
  and turn off when either exceeds ±5° (SRS FR-PFD-002).
- **FR-004**: Attitude MUST stay well-behaved at extremes: pitch limited to the visible scale range, and no
  erratic spinning when the watch is near vertical or upside down.
- **FR-005**: If motion sensing is unavailable, the attitude indicator MUST show a fixed level picture with
  an "ATT FAIL" flag, and every other PFD function MUST keep working.

**Time**

- **FR-006**: A left vertical tape MUST show the local hour (00–23), and a right vertical tape the local
  minute (00–59), each with a highlighted current-value box (SRS FR-PFD-003/004).
- **FR-007**: A top heading tape MUST show the seconds as heading (1 s = 6°, 000–354), scrolling smoothly
  between whole seconds (SRS FR-PFD-005).
- **FR-008**: The bottom corners MUST show ZULU (UTC) and LOCAL time as HH:MM in a high-contrast green
  HUD style (SRS FR-PFD-006).
- **FR-009**: Local time MUST be derived from UTC using a configurable fixed offset (default UTC+3).
  The offset MUST be editable on the Settings page, in 30-minute steps from −12:00 to +14:00.
- **FR-010**: If the watch clock has not been set (or holds an implausible date), all time readouts MUST
  show "--", and a "CLOCK NOT SET" caution MUST be raised.
- **FR-011**: Until automatic time synchronisation arrives (slice S2), the developer MUST be able to set
  the watch clock (UTC) through the diagnostic connection, and the clock MUST keep time across reboots.

**Engine indications (EICAS)**

- **FR-012**: The PFD MUST show FUEL QTY (battery %), FUEL FLOW (drain rate in %/h) and EGT (power-chip
  temperature in °C), refreshed at least once per second (SRS FR-PFD-007).
- **FR-013**: FUEL FLOW MUST be estimated from the battery-percentage trend over a sliding window of at
  least 10 minutes. It MUST show "--" until enough history exists, and a refuel indication while charging.
- **FR-014**: A charging annunciator MUST appear within 2 s of charging starting, and disappear within 2 s
  of it stopping (SRS FR-PFD-009).
- **FR-015**: A "LOW FUEL" caution MUST be raised when the battery is below 15 % and not charging. It MUST
  clear when charging starts or the battery rises above 17 % (hysteresis) (SRS FR-PFD-009).
- **FR-016**: When a power reading is unavailable, the affected gauges MUST show "--".

**Recorder placeholders**

- **FR-017**: The PFD MUST show MAX G and FLIGHT NM readouts reading "--" (to be filled by slice S6)
  (SRS FR-PFD-008).

**Layout, performance & behaviour**

- **FR-018**: The layout MUST fit the T-Watch Ultra's portrait screen, 410 wide × 502 tall (heading tape on
  top, hour and minute tapes beside a centred attitude indicator, EICAS below it, ZULU/LOCAL in a row at the
  bottom), and MUST scale to other screen sizes without overlapping elements. Every element MUST stay inside the
  visible area of the screen's **rounded corners (radius 64 px on the Ultra)**. *(Corrected 2026-11-11: an
  earlier draft assumed a 502×410 landscape screen.)*
- **FR-019**: The hour, minute and corner time digits MUST be at least 24 px tall on the T-Watch Ultra
  (scaled on other screens), readable in daylight (SRS NFR-USE-002).
- **FR-020**: While the PFD is visible, the horizon MUST update smoothly at 30 frames per second or more,
  and lag behind wrist motion by no more than 50 ms (SRS NFR-PERF-001/002).
- **FR-021**: When the PFD is not visible, it MUST NOT keep drawing or sampling faster than other pages
  need. Returning to it MUST show current values immediately.
- **FR-022**: The S0 navigation, caution banner, page indicator and diagnostics overlay MUST keep working
  with the PFD as the home page.

### Key Entities

- **Attitude**: pitch and roll in degrees, level-lock state, and sensor availability. It is continuously
  updated from motion sensing.
- **Clock time**: current UTC instant, the configured local offset, and whether the clock is valid (set).
- **Fuel state**: battery percent, charging flag, power-chip temperature, a short history of battery percent
  over time (for the drain rate), and whether readings are available.
- **Local time offset (setting)**: minutes from UTC, persisted with the other settings.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: With the PFD visible and the wrist moving, the horizon renders at **≥30 frames per second**
  sustained over 60 seconds, as measured by the diagnostics overlay.
- **SC-002**: The horizon follows the wrist with no visible lag. Measured motion-to-display latency is
  ≤50 ms.
- **SC-003**: With the watch held at known angles (0°, ±30° roll; ±10° pitch, checked against a phone level
  app), the displayed roll and pitch match within ±3°.
- **SC-004**: LOCK lights within 1 s of laying the watch flat, in 10 of 10 trials, and does not flicker while
  held level for 30 seconds.
- **SC-005**: Over a 10-minute comparison with a reference clock, the tapes and corner readouts match the
  reference to the second (seconds via the heading box), including one minute rollover and one simulated
  23:59:59 rollover.
- **SC-006**: FUEL QTY matches the diagnostic battery percent within 1 point, and EGT matches the
  diagnostic temperature within 1 °C, in 10 of 10 samples.
- **SC-007**: The charging annunciator appears and disappears within 2 seconds of plugging and unplugging
  USB power, in 5 of 5 trials.
- **SC-008**: A first-time viewer can read the current hour and minute from the PFD within 3 seconds
  (informal hallway test with at least 3 people).
- **SC-009**: Free memory stays above the S0 floor (≥40 KB) after 30 minutes with the PFD visible.

## Assumptions

- The S0 platform (navigation, settings, cautions, diagnostics, serial console) is in place. The PFD replaces
  the S0 placeholder Home page. Placeholder pages A and B remain until later slices replace them.
- "Level" means the watch face pointing straight up (the natural flat position). No user calibration of the
  zero attitude is provided in this slice.
- The hardware clock keeps UTC. Automatic sync (NTP) and full time-zone rules (DST) come in slice S2; until
  then the local offset is a fixed number of minutes, and the clock is set over the diagnostic connection.
- The battery chip cannot measure battery current (found in S0), so FUEL FLOW is an estimated drain rate in
  %/h from the battery-percentage trend, not a current in mA. This replaces the SRS wording "current draw
  (mA)" for FR-PFD-007 (SRS amended in this slice).
- "LOW FUEL" is a visual caution in this slice. Haptic and voice alerting arrive with the alert system (S4),
  which will reuse the same thresholds.
- The screen stays on while the PFD is shown. Screen timeout and raise-to-wake are slice S7.
- Optional true course on the heading tape (SRS FR-PFD-010) is out of scope (slice S12).
- S0 measured full-screen page transitions at 15–28 FPS. Reaching ≥30 FPS for the horizon is an explicit
  goal of this slice, and the plan may change how the screen is drawn to achieve it.
