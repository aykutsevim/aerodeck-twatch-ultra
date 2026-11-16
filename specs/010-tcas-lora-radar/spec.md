# Feature Specification: TCAS LoRa Radar (Slice S9)

**Feature Branch**: `010-tcas-lora-radar`

**Created**: 2026-11-16

**Status**: Draft

**Input**: User description: "S9 on new feat branch"

**Traceability**: AD-SRS-001 FR-TCS-001…004, FR-TCS-006, FR-TCS-008, NFR-REG-001, NFR-SEC-003 (FR-TCS-005 GNSS
bearing → S12; FR-TCS-007 Meshtastic decode → hook only) · SRS open item OI-6 (closed: 915 MHz hardware) · Roadmap milestone M4 (first slice) ·
Depends on S4 (alerts), S7 (power, eject, screen) · Prompt source: `docs/speckit_slice_prompts.md` § S9

## User Scenarios & Testing *(mandatory)*

### User Story 1 - See nearby AeroDeck watches on a radar (Priority: P1)

As the wearer, I want a TCAS radar page with a sweeping plan-position display, where nearby AeroDeck watches (or
compatible LoRa nodes) appear as green "bogeys" placed on range rings by signal strength: stronger means closer.

**Why this priority**: It is the slice's headline feature and the first use of the long-range radio.

**Independent Test**: With TCAS on in two devices, open the radar page on one and check that the other appears
within one beacon interval and moves outward when the devices are carried apart.

**Acceptance Scenarios**:

1. **Given** TCAS is on in both devices, **When** I open the radar page, **Then** the other device appears as a
   bogey within one beacon interval (default 30 s).
2. **Given** a bogey is shown, **When** the other device is carried farther away, **Then** the bogey moves to an
   outer ring (and back inward when it returns).
3. **Given** several devices, **When** they are shown, **Then** each keeps a stable position around the scope (its
   bearing is cosmetic and does not jump between updates), and the sweep line rotates continuously.
4. **Given** a device stops transmitting, **When** 5 minutes pass without hearing it, **Then** its bogey
   disappears.
5. **Given** TCAS is off, **When** I open the radar page, **Then** it shows "TCAS OFF" and a way to turn it on.

---

### User Story 2 - Inspect a contact (Priority: P1)

As the wearer, I want to tap a bogey to see who it is and how good the link is: callsign, signal strength, signal
quality, how long ago it was heard, and its battery level.

**Why this priority**: It turns the dots into information (FR-TCS-004).

**Independent Test**: Tap a bogey and compare the shown values with the other device's settings and the console.

**Acceptance Scenarios**:

1. **Given** a bogey, **When** I tap it, **Then** a detail card shows its callsign, signal strength (dBm), signal
   quality (dB), "heard N s ago" (updating) and its battery %.
2. **Given** the card is open, **When** the contact ages out, **Then** the card closes or shows "LOST".

---

### User Story 3 - Broadcast my own beacon, legally and frugally (Priority: P1)

As the wearer, I want my watch to broadcast a small beacon with my chosen callsign at an adjustable interval
(default 30 s) while TCAS is on, using only legal radio settings for my region, never exceeding the transmit
duty-cycle limit, and with the radio fully powered down when TCAS is off.

**Why this priority**: Without a legal, low-power beacon there is no radar for anyone else (FR-TCS-001/008,
NFR-REG-001, constitution VI and III).

**Independent Test**: Turn TCAS on, check the beacon count and the duty-cycle use over an hour, then turn it off and
check that the radio supply is off.

**Acceptance Scenarios**:

1. **Given** TCAS is on, **When** time passes, **Then** a beacon is sent about every interval (default 30 s, varied
   slightly so that two watches do not keep colliding).
2. **Given** a 1-hour soak with the shortest allowed interval, **When** I check the transmit time used, **Then** it
   never exceeds the region's duty-cycle limit in any rolling hour (EU868: 1 %).
3. **Given** the duty-cycle budget would be exceeded, **When** a beacon is due, **Then** it is delayed or skipped
   (logged), never sent.
4. **Given** I change the region, power or modulation, **When** a value is outside the legal range for the
   region, **Then** it is refused or clamped.
5. **Given** TCAS is turned off (or the watch ejects), **When** I check the radio, **Then** its supply is off and
   nothing is transmitted.
6. **Given** a beacon, **When** it is decoded by anyone, **Then** it contains only the protocol version, callsign,
   sequence number and battery % (no position in this slice, no other personal data).

---

### User Story 4 - TRAFFIC advisory (Priority: P2)

As the wearer, I want a "TRAFFIC" advisory when a new contact appears on the inner ring (very close), so I notice
another AeroDeck wearer nearby even when not looking at the radar.

**Why this priority**: A Should (FR-TCS-006) on top of the radar.

**Independent Test**: Bring a second device very close with TCAS on and check that one TRAFFIC advisory is raised;
keep it close and check it is not repeated.

**Acceptance Scenarios**:

1. **Given** TCAS is on, **When** a contact first appears on (or moves into) the inner ring, **Then** a TRAFFIC
   advisory is raised with the "Traffic" voice callout (respecting quiet mode and quiet hours).
2. **Given** the contact stays on the inner ring, **When** further beacons arrive, **Then** no new advisory is raised
   for it until it has left the inner ring (or aged out) and comes back.
3. **Given** the advisory is shown, **When** I acknowledge it, **Then** it clears like other advisories.

---

### Edge Cases

- **Only one device available**: the radar shows an empty scope with the sweep, the own callsign and the beacon
  count. Reception can be checked against any compatible node (see Assumptions).
- **Two watches with the same callsign**: they are told apart by a device id derived from the hardware; both are
  shown, and the card shows the id suffix.
- **Corrupt or foreign packets** (other LoRa traffic on the channel): ignored and counted; never shown as targets.
- **Strong signal at very short range** (receiver saturates): shown on the inner ring.
- **Very weak signal**: shown on the outermost ring; below the sensitivity limit the contact simply ages out.
- **Radio missing or failing to start**: the page shows "RADIO FAIL" (existing peripheral caution), the rest of the
  watch works.
- **Eject while TCAS is on**: the radio is powered off for deep sleep. After wake, TCAS resumes if it was on.
- **Screen off**: TCAS keeps beaconing and listening. A TRAFFIC advisory turns the screen on (S7 alert wake,
  extended so that a speaking advisory wakes the screen; silent advisories still do not).
- **Region change while running**: the radio is reconfigured. The duty-cycle history is kept (it is per rolling
  hour).
- **Battery cost**: with TCAS on, receiving continuously costs power. The cost is measured and reported. TCAS is off
  by default.

## Requirements *(mandatory)*

### Functional Requirements

**Beacon and radio**
- **FR-001**: While TCAS is on, the watch MUST send a beacon containing protocol version, callsign, sequence number
  and battery %, at a configurable interval (default 30 s, range 15 s–5 min) with ±10 % random variation.
  (FR-TCS-001, NFR-SEC-003)
- **FR-002**: The radio region (default EU868; US915 selectable), transmit power, spreading factor and bandwidth
  MUST be configurable and limited to the legal values of the selected region. The frequency is the region's fixed
  channel (EU868: 869.525 MHz) and is shown, not edited. (FR-TCS-008)
- **FR-003**: The watch MUST enforce the region's transmit duty-cycle limit over any rolling hour (EU868: ≤ 1 %).
  A beacon that would exceed it MUST be delayed or skipped, never sent. (NFR-REG-001)
- **FR-004**: The radio supply MUST be on only while TCAS is on. TCAS MUST be off by default, and its on/off state
  MUST persist and survive eject. (NFR-PWR-001)
- **FR-005**: The callsign MUST be settable (1–8 letters, digits or dashes; default derived from the device id).

**Reception and targets**
- **FR-006**: While TCAS is on, the watch MUST receive beacons in the background and keep a table of up to 16
  contacts (device id, callsign, last signal strength and quality, last seen, battery %). Contacts MUST age out after
  a configurable time (default 5 min). Invalid packets MUST be ignored and counted. (FR-TCS-002)

**Radar page**
- **FR-007**: A TCAS page MUST show a sweeping radar with range rings. Each contact is drawn as a bogey on a ring
  derived from its smoothed signal strength (stronger = closer), at a bearing that is stable per contact. The page
  MUST show the own callsign, the TCAS state and the number of contacts. (FR-TCS-003)
- **FR-008**: Tapping a bogey MUST show callsign, signal strength, signal quality, age and battery %. (FR-TCS-004)
- **FR-009**: The page MUST offer a TCAS on/off control, and MUST follow NVG mode.

**Advisory**
- **FR-010**: A contact that enters the inner ring MUST raise one TRAFFIC advisory with the "Traffic" callout (quiet
  rules apply). It MUST NOT be raised again for that contact until the contact has left the inner ring or aged out.
  (FR-TCS-006)

**Diagnostics and quality**
- **FR-011**: The console MUST show the TCAS state, radio settings, duty-cycle use, beacon and receive counters and
  the contact table, and MUST offer test hooks: send a beacon now, inject a simulated contact, and set a test
  interval.
- **FR-012**: The beacon encoding and decoding, the duty-cycle budget, the signal-to-ring mapping (with smoothing),
  the region limits and the contact table ageing MUST be tested off-device. (NFR-MNT-004)
- **FR-013**: TCAS MUST NOT reduce the watch face's smoothness below a median of 30 fps. The radar page's own
  animation MUST stay smooth (sweep visibly continuous).
- **FR-014**: The extra battery drain of TCAS on (screen off) MUST be measured and added to the power report.

### Key Entities

- **Beacon**: protocol version, device id, callsign, sequence number, battery %. It has a few dozen bytes and no
  position.
- **Contact**: device id, callsign, smoothed signal strength, last quality, last seen, battery %, ring, stable
  bearing, inner-ring flag.
- **Radio settings**: region (which fixes the frequency), power, spreading factor, bandwidth (legal per region); TCAS on/off;
  callsign; beacon interval; contact age-out time.
- **Duty-cycle budget**: the transmit time used in the last rolling hour vs the region's limit.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: With two devices with TCAS on, each sees the other within one beacon interval in 5 of 5 starts.
  *(Pending manual: needs a second device. On this watch: a simulated contact appears within 1 s of injection.)*
- **SC-002**: Carrying one device from about 2 m to about 100 m away moves its bogey outward by at least one ring,
  and back inward when it returns. *(Pending manual: needs a second device. On this watch: injected signal
  strengths map to the expected rings.)*
- **SC-003**: In a 1-hour soak with a 5 s test interval (set through the console test hook, below the 15 s user
  minimum, so the budget is certain to bind), the transmit time in every rolling hour stays ≤ the region limit
  (EU868 ≤ 36 s), and no beacon is sent over budget.
- **SC-004**: With TCAS off, the radio supply is off (verified on the device), and the screen-off current is the same
  as before S9.
- **SC-005**: A contact entering the inner ring raises exactly one TRAFFIC advisory per entry in 3 of 3 trials
  (verified with simulated contacts on this watch; with a real second device when available).
- **SC-006**: The watch face keeps a median of ≥ 30 fps with TCAS on; the radar sweep looks continuous (wearer).
- **SC-007**: The extra drain of TCAS on (screen off) is measured and documented, with its effect on the battery
  estimate.
- **SC-008**: On this watch, beacons are actually transmitted at the legal EU868 frequency (transmit-done for every
  beacon, the beacon counter matching the interval over 10 minutes), and the receiver is shown to be listening
  (it counts foreign packets if any LoRa traffic is present; otherwise this is noted).

## Assumptions

- **Second device for testing**: none is available (wearer, 2026-11-16).
  - The two-device criteria (SC-001, SC-002, SC-005) are **pending manual checks** until a second AeroDeck watch or
    LoRa node is available.
  - On this watch, the slice is verified with:
    - transmit evidence (beacon counter, transmit-done, duty-cycle log);
    - the radio supply state;
    - **simulated contacts** injected through the console (radar, card, rings, TRAFFIC);
    - the receive path, checked by counting any foreign LoRa packets heard on the channel.
- **Radio band of this unit**: the wearer reports the **915 MHz (US)** hardware variant (closes SRS OI-6).
  - The wearer is in Türkiye, where only the EU868 band (863–870 MHz) is permitted for this use. The 902–928 MHz
    band is not.
  - The radio chip can tune to 868 MHz, so the watch operates in the **legal EU868 band** (default 869.525 MHz) on
    hardware whose antenna and filter are tuned for 915 MHz. Range will be shorter than on 868 MHz hardware; this is
    a documented hardware limitation.
  - The default transmit power is low, and the region limits always apply.
  - US915 is offered only for use where that band is legal (the wearer selects the region; the default stays
    EU868).
- **Bearing**: without position data, bearing is cosmetic but stable per contact. True bearing with GNSS is slice S12
  (FR-TCS-005).
- **Meshtastic**: passive decoding (FR-TCS-007, Could) is out of scope; only a code hook is left.
- **Position**: no position is sent in this slice. The opt-in position field comes with S12.
- **TRAFFIC voice**: S5 kept advisories silent. TRAFFIC is the one advisory that speaks, once per entry, because the
  "Traffic" clip exists for it (S5).
- **T-Watch S3**: must compile. The S3 has no LoRa radio (S0 caps), so TCAS shows "RADIO N/A" there.
- **Out of scope**: messaging, relaying or meshing, encryption, GNSS positions, Meshtastic decoding.

## Dependencies

- S0 (merged): rail leases (`Lora` rail), peripheral status, settings, page ring, console.
- S4 (merged): alerts (advisory level), haptics. S5 (merged): "Traffic" voice clip, quiet rules.
- S7 (merged): eject sequence (power the radio off), screen alert wake, NVG.
- S2 (merged): not required; beacons do not need the clock.
