# Feature Specification: Time Sync & Connectivity (Slice S2)

**Feature Branch**: `003-time-sync`

**Created**: 2026-11-11

**Status**: Draft

**Input**: User description: "S2 on new feat branch, after each slice finishes mark in product roadmap document, add this rule to product roadmap"

**Traceability**: AD-SRS-001 FR-TIM-001…004, FR-TIM-003 (replaces the S1 interim fixed offset), FR-TIM-006, FR-TIM-007
(kept as a fallback), FR-NET-001/002, NFR-SEC-001 · Roadmap milestone M1 · Depends on S0, S1 (merged) ·
FR-TIM-005 (GNSS time) stays with slice S12 · Prompt source: `docs/speckit_slice_prompts.md` § S2

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Connect the watch to my Wi-Fi (Priority: P1)

As the wearer, I give the watch the name and password of my Wi-Fi network once, and it remembers them across
reboots. I can see whether it is configured and whether the last connection worked, and I can forget the
network again. My password is never shown in full and never appears in any diagnostic output.

**Why this priority**: Automatic time and later weather (slice S3) both depend on network access.

**Independent Test**: Enter the credentials, reboot, and confirm the watch reports the network as configured and
can connect. Then forget it and confirm it no longer tries.

**Acceptance Scenarios**:

1. **Given** no network is configured, **When** I enter a network name and password, **Then** the watch saves them
   and confirms with a test connection result (connected / wrong password / network not found) within 20 seconds.
2. **Given** a network is saved, **When** the watch reboots, **Then** the network is still configured.
3. **Given** a network is saved, **When** I view the settings or any diagnostic output, **Then** the password is
   masked (e.g. `••••`), never shown.
4. **Given** a network is saved, **When** I choose "Forget network" and confirm, **Then** the credentials are erased
   and the watch stops trying to connect.
5. **Given** I enter a wrong password, **When** the test connection runs, **Then** I'm told it failed and why, and the
   watch keeps working normally.

How credentials are entered *(clarified 2026-11-11)*: **from a phone through a temporary setup hotspot**, plus a
**diagnostic command over USB** for development. There is no on-watch keyboard in S2.

6. **Given** I choose "Set up Wi-Fi" on the watch, **When** the setup hotspot starts, **Then** the watch shows the
   hotspot name, a one-time access code, and the address of the setup page.
7. **Given** my phone has joined the setup hotspot with the access code, **When** I open the setup page, **Then** I
   see a list of nearby networks (strongest first) and can pick one or type a name, enter the password and save.
8. **Given** I saved credentials on the setup page, **When** the watch receives them, **Then** the hotspot closes, the
   test connection runs, and the watch shows the result.
9. **Given** the setup hotspot is open, **When** 5 minutes pass without saved credentials (or I cancel on the watch),
   **Then** the hotspot closes and the radio turns off.
10. **Given** a computer is connected over USB, **When** I send the network name and password with the diagnostic
    command, **Then** they are saved and tested exactly as from the setup page.

---

### User Story 2 - Accurate time without thinking about it (Priority: P1)

As the wearer, my watch sets its clock from internet time when it starts and then every hour. It saves the time
to its hardware clock and turns the radio off again straight away. I never need to set the time by hand, and the
clock stays correct even when no network is around.

**Why this priority**: Correct time is the watch's main job. In S1, time had to be set by hand over the cable.

**Independent Test**: With a network configured, deliberately set the clock wrong, reboot, and check the clock is
corrected within a minute. Leave it running and check an hourly sync happens. Check the radio is off between syncs.

**Acceptance Scenarios**:

1. **Given** a configured network in range, **When** the watch boots, **Then** its clock is corrected from internet time
   within 30 seconds, and any CLOCK NOT SET caution clears.
2. **Given** the watch has synced, **When** an hour passes, **Then** it syncs again automatically (interval
   adjustable).
3. **Given** a sync has finished (success or failure), **When** I check the radio state, **Then** Wi-Fi is off.
4. **Given** no network is reachable, **When** a sync is attempted, **Then** the attempt gives up within 15 seconds,
   the display never freezes, and the existing clock keeps running.
5. **Given** the clock was synced and the watch then reboots without network, **When** it starts, **Then** it shows
   the correct time from its hardware clock.
6. **Given** I want an immediate sync, **When** I choose "Sync now", **Then** a sync runs and reports its result.

---

### User Story 3 - Local time with real time-zone rules (Priority: P2)

As the wearer, I choose my time zone by name (e.g. "Istanbul", "London", "New York") instead of a raw offset.
LOCAL time then follows that zone's rules, including summer-time changes, while ZULU stays UTC. The default is
Türkiye (UTC+3, no summer time).

**Why this priority**: The S1 fixed offset is wrong half the year for zones with summer time.

**Independent Test**: Select a zone with summer time, set the clock to just before a changeover, and check LOCAL
jumps by an hour at the right moment while ZULU continues smoothly.

**Acceptance Scenarios**:

1. **Given** zone "Istanbul" is selected, **When** UTC is 21:00, **Then** LOCAL shows 00:00.
2. **Given** zone "London" is selected, **When** the clock passes the last Sunday in March 01:00 UTC, **Then** LOCAL
   jumps from 00:59 to 02:00, and ZULU is unaffected.
3. **Given** I change the zone, **When** I return to the watch face, **Then** LOCAL reflects the new zone within 1 second.
4. **Given** the watch was using an S1 fixed offset, **When** S2 firmware first starts, **Then** the previous local time
   is preserved by selecting a matching zone (or an equivalent fixed-offset zone if no named zone matches).

---

### User Story 4 - Know when time might be stale (Priority: P3)

As the wearer, I can see when the clock was last synced. If it hasn't synced for more than 24 hours, a "TIME UNSYNC"
advisory appears, so I know to check my network settings.

**Why this priority**: The hardware clock drifts slowly, so this only matters after long periods without network.

**Independent Test**: Simulate a last sync more than 24 hours ago and confirm the advisory, then sync and confirm it clears.

**Acceptance Scenarios**:

1. **Given** the last successful sync was 25 hours ago, **When** I look at the watch, **Then** a TIME UNSYNC advisory is shown.
2. **Given** TIME UNSYNC is shown, **When** a sync succeeds, **Then** the advisory clears.
3. **Given** the Settings page is open, **When** I look at the time section, **Then** I see the time since the last
   successful sync (e.g. "12 min ago", "never").

### Edge Cases

- **Network present but no internet** (e.g. a hotel captive portal): the sync fails within the time limit, the clock
  is untouched, and the failure reason is reported as "no time response".
- **Time server returns an implausible time** (year before 2025): it is rejected, and the clock is not changed.
- **Sync while the clock is far off** (e.g. a never-set RTC): the clock jumps to the correct time. The watch face
  shows the new time at once, without stale readouts.
- **Sync attempt during heavy UI use** (e.g. swiping pages): no visible stutter beyond normal.
- **Credentials changed during a running sync**: the running attempt finishes or times out, and the next attempt uses the new credentials.
- **Low battery** (LOW FUEL active): scheduled syncs still run. Battery-aware scheduling is a later concern (S7).
- **Summer-time changeover while the watch is off**: on next start, LOCAL is correct for the current date.
- **Very long network names or passwords** (up to the Wi-Fi limits of 32 and 63 characters) are accepted and stored correctly.

## Requirements *(mandatory)*

### Functional Requirements

**Wi-Fi credentials**

- **FR-001**: The watch MUST store one Wi-Fi network (name and password) persistently, and MUST allow replacing or
  forgetting it (SRS FR-PLT-005/006).
- **FR-002**: The password MUST never be displayed in full or written to any diagnostic output. Wherever it would
  appear it MUST be masked (SRS NFR-SEC-001).
- **FR-003**: After credentials are entered, the watch MUST run a test connection and report one of: connected, wrong
  password, network not found, no time response, timed out, time rejected (implausible), within 20 seconds.
- **FR-004**: Credentials MUST be enterable from a phone through a **temporary setup hotspot** started from the
  Settings page, and through a **diagnostic USB command**. *(Clarified 2026-11-11.)*
- **FR-004a**: The setup hotspot MUST start only on a deliberate action on the watch. It MUST be protected by a
  one-time access code shown only on the watch screen, MUST close automatically after 5 minutes or when credentials
  are saved or setup is cancelled, and MUST NOT run at the same time as a sync (constitution VI).
- **FR-004b**: The setup page MUST list nearby networks (name and signal strength, strongest first), allow typing a
  hidden network's name, and accept passwords up to 63 characters. It MUST NOT display a saved password.
- **FR-004c**: The setup page MUST work on current iOS and Android phone browsers without installing an app.

**Time synchronisation**

- **FR-005**: With a network configured, the watch MUST sync its clock from internet time at boot, and then at a
  configurable interval (default 60 minutes, range 15 minutes – 24 hours) (SRS FR-TIM-002).
- **FR-006**: Every successful sync MUST also update the hardware clock, so time survives reboots without network
  (SRS FR-TIM-001/002).
- **FR-007**: Wi-Fi MUST be powered only during a sync transaction or a test connection, and MUST be off at all other
  times (SRS FR-NET-001).
- **FR-008**: Each network transaction MUST give up after at most 15 seconds, and MUST never freeze or visibly slow the
  watch face (SRS FR-NET-002).
- **FR-009**: Times before 2025 or otherwise implausible MUST be rejected without changing the clock.
- **FR-010**: A successful sync MUST clear the CLOCK NOT SET caution (SRS FR-TIM-006).
- **FR-011**: The wearer MUST be able to trigger an immediate sync ("Sync now") from the Settings page and from the
  diagnostic connection, and see its result.
- **FR-012**: The manual clock-set command from S1 MUST remain available as a fallback for use without any network
  (SRS FR-TIM-007).

**Time zones**

- **FR-013**: Local time MUST follow a selectable named time zone with its summer-time rules (SRS FR-TIM-003). At least
  40 zones covering every UTC offset in use MUST be offered, including Istanbul (the default), London, Berlin, New York,
  Los Angeles, Tokyo, Kolkata (+5:30) and Sydney.
- **FR-014**: Changing the zone MUST take effect on all time displays within 1 second, and MUST persist.
- **FR-015**: On first start after upgrading from S1, the previous fixed offset MUST be converted to an equivalent zone,
  so LOCAL time doesn't change.
- **FR-016**: ZULU MUST always show UTC, unaffected by the zone.

**Sync status**

- **FR-017**: The Settings page MUST show whether a network is configured, the result of the last attempt, and the time
  since the last successful sync (SRS FR-TIM-004).
- **FR-018**: A "TIME UNSYNC" advisory MUST appear when the last successful sync is more than 24 hours old, or has never
  happened while a network is configured, and MUST clear on the next successful sync (SRS FR-TIM-004).

### Key Entities

- **Network credentials**: network name (≤32 characters) and password (≤63 characters, secret).
- **Setup session**: the temporary hotspot's name, its one-time access code, its expiry time, and the networks found nearby.
- **Sync status**: time of the last successful sync, result of the last attempt (success or failure reason), next scheduled attempt.
- **Time zone**: a named zone (display name and summer-time rules). It replaces the S1 fixed offset setting.
- **Sync interval**: minutes between scheduled syncs.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: With a configured network in range, the clock is correct to within 1 second of a reference within 30
  seconds of boot, in 10 of 10 boots.
- **SC-002**: Over 24 hours with the network available, 24 of 24 scheduled hourly syncs succeed, and the clock never
  differs from the reference by more than 1 second.
- **SC-003**: Between syncs, the Wi-Fi radio is off 100 % of the time (verified from the watch's power/diagnostic log).
- **SC-004**: With no network reachable, 10 of 10 sync attempts give up within 15 seconds, and the watch face stays at
  **≥28 frames per second** during the attempts (S1 baseline: median 30).
- **SC-005**: The Wi-Fi password appears in none of: the Settings page, diagnostic output, logs (checked over 24 hours of logs).
- **SC-006**: Summer-time changeovers in a selected zone happen at the correct instant in 2 of 2 tests (spring and autumn).
- **SC-007**: A first-time user can configure Wi-Fi from a phone and see the time synced within 3 minutes.
- **SC-008**: The setup hotspot is never active for more than 5 minutes per session, and is off 100 % of the time
  outside a setup session.

## Assumptions

- One saved network is enough for S2. Multiple networks are a possible later enhancement.
- No on-watch keyboard in S2 (product-owner decision). The phone setup hotspot is the user path; the USB command is
  the developer path.
- WPA2/WPA3-Personal networks only (password 8–63 characters). Open networks, Enterprise (802.1X) and captive-portal
  login pages are out of scope.
- The default time servers are public internet time servers. A custom server setting is optional, not required.
- The zone list is built into the firmware (no download). Rules follow current published summer-time rules. A future
  rule change needs a firmware update.
- GNSS-based time (SRS FR-TIM-005) stays with slice S12.
- Battery-aware sync scheduling (skipping syncs when battery is low) is out of scope until S7.
- The METAR weather fetch (S3) will reuse the same network transaction, but is not part of this slice.
- The S1 `tzoff` setting is replaced by the zone setting. Its value is used once to pick the initial zone (FR-015).
