# Feature Specification: METAR Weather Page (Slice S3)

**Feature Branch**: `004-metar-weather`

**Created**: 2026-11-11

**Status**: Draft

**Input**: User description: "S3 on new feat branch"

**Traceability**: AD-SRS-001 FR-WX-001…005, NFR-SEC-002, NFR-MNT-004 · Roadmap milestone M1 (closes it) ·
Depends on S0, S1, S2 (merged; S2 provides the Wi-Fi connection and the hourly sync) ·
Prompt source: `docs/speckit_slice_prompts.md` § S3

## User Scenarios & Testing *(mandatory)*

### User Story 1 - See my home airport's weather at a glance (Priority: P1)

As the wearer, I swipe to the weather page and see the latest aviation weather report (METAR) for my home airport
(default **LTAC**, Ankara Esenboğa), decoded into large, readable fields:
- the station and the observation time in Zulu;
- the wind direction (°) and speed in knots, with gusts if any;
- the visibility;
- the QNH pressure in hPa;
- the temperature and dewpoint;
- a colour-coded **flight category**: VFR green, MVFR blue, IFR red, LIFR magenta.

The weather is fetched automatically during the watch's hourly time sync, so I don't have to do anything.

**Why this priority**: This is the slice's core value. With it, the M1 "usable daily watch" milestone is complete.

**Independent Test**: With Wi-Fi configured, reboot the watch, wait for the first sync, and open the weather page.
Compare every decoded field with the raw report from the weather provider's website for the same station and time.

**Acceptance Scenarios**:

1. **Given** Wi-Fi is configured and the network is reachable, **When** the watch completes its boot or hourly time
   sync, **Then** it also fetches the station's latest report in the same radio session, and the radio is off again
   afterwards.
2. **Given** a report has been fetched, **When** I open the weather page, **Then** I see the station, the observation
   time (Z), the wind, the visibility, QNH (hPa), temperature/dewpoint and the flight category, and every value matches
   the raw report.
3. **Given** a report with gusts (e.g. `27015G25KT`), **When** it is shown, **Then** the wind reads 270° 15 kt
   gusting 25.
4. **Given** a report with variable wind (`VRB03KT`, or a variable sector such as `250V310`), **When** it is shown,
   **Then** the wind direction reads "VRB" (and the sector, when given) instead of a single bearing.
5. **Given** a report with `CAVOK`, **When** it is shown, **Then** the visibility reads "CAVOK" (≥10 km, no
   significant cloud), and the category is VFR.
6. **Given** a report that reports pressure in inches of mercury (`A2992`), **When** it is shown, **Then** QNH is
   converted and shown in hPa (1013).
7. **Given** a report where a field is missing or not reported (e.g. `/////` visibility, no temperature group),
   **When** it is shown, **Then** that field shows "--", and the other fields are still decoded.
8. **Given** the reported ceiling and visibility, **When** the flight category is computed, **Then** it follows the
   standard thresholds (see FR-007), and it shows "--" in neutral grey when the data is not enough to decide
   (FR-007 unknown rule).

---

### User Story 2 - Know when the weather is old or the fetch failed (Priority: P1)

As the wearer, I can trust what I see. If the report is more than 90 minutes old, it is clearly marked **STALE**. If
the last fetch failed, the page keeps showing the last good report with an error indicator that says why (no Wi-Fi,
network failed, no report for this station, server error). After a reboot, the last good report is still shown before
any new fetch.

**Why this priority**: An old or wrong weather report that looks current is worse than none.

**Independent Test**: Fetch a report, then make the network unreachable. Trigger a refresh and confirm the old report
stays with an error indicator. Wait (or simulate) until the observation is >90 min old and confirm STALE. Reboot
with no network and confirm the cached report is shown.

**Acceptance Scenarios**:

1. **Given** the shown report's observation time is more than 90 minutes before the current UTC time, **When** the
   page is visible, **Then** a STALE marker is shown. It disappears as soon as a newer report arrives.
2. **Given** a good report is shown, **When** a fetch fails, **Then** the report stays and an error indicator shows
   the failure reason. The next successful fetch clears the indicator.
3. **Given** a good report was fetched before, **When** the watch reboots with no network, **Then** the page shows
   that report (with STALE if it is old), not an empty page.
4. **Given** no report has ever been fetched, **When** I open the page, **Then** it shows the station and "NO DATA"
   with the reason (e.g. "Wi-Fi not set up").
5. **Given** the watch's clock is not set, **When** the page is shown, **Then** the report's age cannot be judged and
   the page shows it as STALE (it never claims fresh data it can't verify).

---

### User Story 3 - Refresh now (Priority: P2)

As the wearer, before a flight or a walk, I tap a refresh button on the weather page to fetch the latest report
immediately. I see that it is updating, the page stays responsive while it runs, and the result (new report or
failure reason) appears within 20 seconds.

**Why this priority**: Hourly updates are enough most of the time, but a manual refresh helps when it matters.

**Independent Test**: Tap refresh on the weather page while moving the wrist. Confirm a "refreshing" indication, a
smooth display, and an updated fetch time or a failure reason within 20 s.

**Acceptance Scenarios**:

1. **Given** Wi-Fi is configured, **When** I tap refresh, **Then** a "refreshing" indication appears within 100 ms,
   and the new report (or a failure reason) appears within 20 s.
2. **Given** a refresh is running, **When** I tap refresh again, **Then** nothing extra happens (no second fetch).
3. **Given** a time sync is running, **When** I tap refresh, **Then** the weather fetch runs right after it, in the
   same radio session if possible. It never overlaps another network transaction or the setup hotspot.
4. **Given** Wi-Fi is not configured, **When** I tap refresh, **Then** the page says "Wi-Fi not set up" and no fetch
   is attempted.
5. **Given** a refresh is running, **When** I swipe to another page, **Then** the swipe and the other page are as
   smooth as without the refresh.

---

### User Story 4 - Change the station (Priority: P2)

As the wearer, I change the weather station to another airport, e.g. LTFM (Istanbul) when I travel. I pick the four
letters on the watch in Settings, or send them over USB. The new station is fetched right away and is remembered
across reboots.

**Why this priority**: Most wearers have one home airport; changing it is occasional but necessary.

**Independent Test**: In Settings, change the station to another ICAO code. Confirm the weather page fetches and
shows that station's report. Reboot and confirm the station is kept.

**Acceptance Scenarios**:

1. **Given** I am in Settings, **When** I open "Weather station", **Then** I can pick four letters (A–Z) and confirm,
   or cancel without change.
2. **Given** I confirm a new station, **When** Wi-Fi is configured, **Then** a fetch for the new station starts
   immediately. The previous station's report is no longer shown as current (the page shows the new station with
   "NO DATA" until it arrives).
3. **Given** I confirm a station code that has no reports (e.g. a non-existent airport), **When** the fetch
   completes, **Then** the page says "No report for XXXX", and the station setting is kept so I can correct it.
4. **Given** a computer is connected over USB, **When** I send a station code, **Then** it is validated (exactly four
   letters A–Z, case-insensitive) and applied exactly as from Settings. Anything else is rejected with an error.
5. **Given** a station was changed, **When** the watch reboots, **Then** the station is kept.

---

### User Story 5 - Wind on a compass and the raw report (Priority: P3)

As the wearer, I see the wind as an arrow on a small compass rose (direction relative to north), and I can read the
full raw METAR text on the page, for the details the decoder doesn't show (weather phenomena, remarks, trends).

**Why this priority**: The decoded fields already answer the main question. The compass and raw text add confidence
and detail.

**Independent Test**: Compare the compass arrow with the decoded wind direction for several reports, and the raw text
with the provider's raw report.

**Acceptance Scenarios**:

1. **Given** a report with wind 270° 15 kt, **When** the page is shown, **Then** the compass arrow points from the west
   (wind *from* 270°), with north up.
2. **Given** calm (`00000KT`) or variable wind, **When** the page is shown, **Then** the compass shows "CALM" or "VRB"
   instead of an arrow.
3. **Given** a report, **When** I look at the raw text, **Then** it is shown in full, exactly as received; long text
   wraps or scrolls so none of it is cut off, including inside the screen's rounded corners.

---

### Edge Cases

- **Report formats:** `KT` and `MPS` wind units (MPS converted to kt); visibility in metres (`9999` = 10 km or more),
  statute miles including fractions (`1 1/2SM`, `M1/4SM`, `P6SM`), and directional minimum visibility (decoder uses
  the prevailing value); `CAVOK`; `NSC`/`NCD`/`SKC`/`CLR`; vertical visibility `VV002` (counts as ceiling);
  `M`-prefixed negative temperatures (`M05/M10`); `AUTO`, `COR`, `NIL` reports; remarks and trend groups (`NOSIG`,
  `BECMG`, `TEMPO`) are ignored by the decoder but stay in the raw text.
- **NIL report** (`METAR LTAC 111020Z NIL`): treated as "No report", the last good report is kept.
- **Provider returns several reports:** the newest observation is used.
- **Provider response is malformed, empty or too large:** treated as a server error; the last good report is kept;
  the watch never crashes or blocks.
- **Provider unreachable or slow:** the fetch gives up within its 15 s limit (FR-003); the radio turns off.
- **The setup hotspot is open:** no weather fetch runs; queued requests run after it closes.
- **Station changed while a fetch for the old station is running:** the old result is discarded, and the new station
  is fetched next.
- **Wi-Fi forgotten:** the cached report remains visible (with STALE as it ages) with "Wi-Fi not set up".
- **Clock jumps** (time sync sets the clock): staleness is re-evaluated with the new time.
- **Display:** values must fit inside the 410×502 rounded-corner (r = 64 px) screen at every length (e.g.
  "VRB 250V310 03 kt G15", "1 1/2 SM", "-12/-15").
- **Data received from the network is untrusted:** fields are length-checked; an over-long or unexpected value is
  treated as missing, never shown truncated in a way that changes its meaning.

## Requirements *(mandatory)*

### Functional Requirements

**Fetching**
- **FR-001**: The watch MUST fetch the latest METAR for the configured station in every scheduled sync session (boot and
  hourly, S2) in which the network connection succeeds, in the same radio session (even if the time query fails), and on demand (refresh button, station change, diagnostic command).
  (FR-WX-001)
- **FR-002**: Weather fetches MUST use an encrypted connection (HTTPS) to the weather provider, and MUST verify the
  provider's identity (server certificate). An unverifiable server is a fetch failure. (NFR-SEC-002)
- **FR-003**: Time limits: an **on-demand** fetch (refresh, station change, diagnostic command) MUST give up within
  15 s of starting, **including** bringing up the Wi-Fi connection. The weather step inside a scheduled sync session
  MUST give up within 15 s of starting that step (the whole session then stays ≤30 s). A weather fetch MUST never overlap another network
  transaction or the setup hotspot, and the radio MUST be off afterwards (S2 rule: Wi-Fi off between transactions).
- **FR-004**: A failed weather fetch MUST NOT affect the time sync result or schedule, and vice versa.

**Decoding and display**
- **FR-005**: The watch MUST decode from the raw METAR: the station, the observation day/time (Z), the wind direction
  (or VRB/variable sector/calm), speed and gusts (kt; MPS converted), the prevailing visibility (metres or statute
  miles, or CAVOK), cloud layers (to find the ceiling), the temperature and dewpoint (°C, including negatives), and
  QNH (hPa; inHg converted, rounded to whole hPa). (FR-WX-002)
- **FR-006**: The weather page MUST show the station, the observation time (Z), the wind, the visibility, QNH, the
  temperature/dewpoint and the flight category. A missing or undecodable field MUST show "--" without hiding the
  others. (FR-WX-002)
- **FR-007**: The flight category MUST be computed from the ceiling (lowest broken/overcast layer or vertical
  visibility) and the visibility, using the worse of the two:
  - **LIFR:** ceiling < 500 ft or visibility < 1 SM (1600 m);
  - **IFR:** ceiling 500–999 ft or visibility 1 to < 3 SM (1600 to < 4800 m);
  - **MVFR:** ceiling 1000–3000 ft or visibility 3–5 SM (4800–8000 m);
  - **VFR:** otherwise. No ceiling counts as unlimited; CAVOK counts as VFR.

  The colours are VFR green, MVFR blue, IFR red, LIFR magenta. **Unknown rule:** when the visibility is unknown, the
  category MUST show "--" in neutral grey. When the cloud information is missing (no cloud group and no
  CAVOK/NSC/NCD/SKC/CLR), the category MUST show "--", unless the visibility alone already gives LIFR, which is
  then shown (a ceiling can only make the category worse, and LIFR is already the worst; IFR from visibility alone is
  not shown because an unreported low ceiling could make it LIFR).
- **FR-008**: The weather page MUST show the full raw METAR text exactly as received, readable in full (wrapped or
  scrollable) inside the screen's visible area. (FR-WX-003)
- **FR-009**: The weather page SHOULD show a compass rose with north up and an arrow for the wind direction (wind
  *from*), with "CALM" or "VRB" text instead of an arrow when there is no single direction. (FR-WX-005)
- **FR-010**: The weather page MUST also show the cloud layers in short form (e.g. "FEW030 BKN045") or "NSC"/"CAVOK",
  and the time the report was fetched.

**Freshness and failures**
- **FR-011**: The page MUST mark the report **STALE** when its observation time is more than 90 minutes before the
  current UTC time, or when the current time is not set. The marker MUST update while the page is visible (within
  60 s of crossing the limit). (FR-WX-004)
- **FR-012**: When a fetch fails, the last good report MUST stay shown, with an error indicator naming the reason:
  "Wi-Fi not set up", "Network failed", "No report for XXXX", "Server error". A successful fetch clears it. (FR-WX-004)
- **FR-013**: The last good report MUST be kept across reboots and shown at boot before any new fetch. It MUST be
  discarded when the station changes.
- **FR-014**: With no report available, the page MUST show the station and "NO DATA" with the reason.

**Refresh and station**
- **FR-015**: The weather page MUST have a refresh button. A tap MUST show a "refreshing" indication within 100 ms and
  request a fetch. Taps while a fetch is pending or running MUST NOT queue additional fetches.
- **FR-016**: The station MUST be a four-letter ICAO code (A–Z), default **LTAC**, kept across reboots. It MUST be
  changeable from Settings (a four-letter picker with confirm/cancel) and over the USB diagnostic console. Invalid
  codes MUST be rejected. A change MUST trigger a fetch when Wi-Fi is configured.
- **FR-017**: The diagnostic console MUST offer commands to show the current weather state (station, decoded fields,
  age, last result), to fetch now, and to set the station. It MUST also offer a way to decode a METAR text given on
  the command line without fetching, for testing.

**Quality**
- **FR-018**: The decoder and flight-category logic MUST be tested off-device with at least 20 real METAR reports,
  covering gusts, variable wind and sectors, calm, MPS, CAVOK, statute-mile fractions, vertical visibility, negative
  temperatures, inHg pressure, missing fields, AUTO, NIL and trend/remark groups. (NFR-MNT-004)
- **FR-019**: Fetching and decoding MUST NOT reduce the smoothness of the watch face or page transitions beyond the
  limits already accepted for the S2 time sync.
- **FR-020**: Network data MUST be treated as untrusted: the response size is bounded, every decoded field is
  range-checked, and malformed input never crashes or blocks the watch.

### Key Entities

- **Weather station**: the four-letter ICAO code the wearer chose (default LTAC). Persisted setting.
- **Weather report**: one METAR for the station: the raw text, the observation time (UTC), the decoded fields (wind,
  visibility, clouds/ceiling, temperature, dewpoint, QNH) with a "missing" state for each, the flight category, and
  the time it was fetched. The last good one is persisted.
- **Weather status**: the state of fetching: idle / pending / fetching, the last result (OK or a failure reason), and
  when it happened. Not persisted.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For 20 of 20 real reports (from the off-device test set and live fetches), every decoded field and the
  flight category match a manual decode of the raw text.
- **SC-002**: With the network available, a fresh report (≤90 min old) for the configured station is on the page
  within 30 s after boot, in 10 of 10 boots.
- **SC-003**: A tap on refresh shows a "refreshing" indication within 100 ms, and the result within 20 s, in 10 of 10
  tries. During those fetches, the watch face and page transitions stay at the S2-accepted smoothness (median ≥28
  frames per second).
- **SC-004**: With the network unreachable, 10 of 10 on-demand fetch attempts give up within 15 s of the request, the last good report stays
  visible with an error indicator, and the radio is off afterwards.
- **SC-005**: After a reboot with no network, the last good report is visible within 5 s of the watch face appearing.
- **SC-006**: Over 24 hours with the network available, 24 of 24 hourly fetches succeed, the radio is off between
  fetches, and the page never shows a report older than 90 minutes without the STALE marker.
- **SC-007**: Changing the station from Settings takes under 30 s for a wearer who knows the code, and the new
  station's report is shown within 20 s after confirming.

## Assumptions

- **Provider:** the public `aviationweather.gov` data service, which needs no account or key. A second provider that
  needs a user API key (CheckWX, mentioned in the SRS as optional) is **out of scope** for S3, and may come later.
- **Report kind:** only METAR (and SPECI, treated the same) is in scope. TAF forecasts, NOTAMs, other stations and
  maps are out of scope.
- **Decoding:** present-weather phenomena (RA, TS, …), runway visual range, remarks and trend groups are not decoded;
  they remain visible in the raw text.
- **Units:** wind in knots, pressure in hPa, temperature in °C, visibility as reported (metres/kilometres or statute
  miles), cloud heights in feet. Unit preferences are out of scope.
- **Flight category thresholds:** the standard US categories as used by the provider (FR-007). Values exactly on a
  boundary fall into the better category (e.g. ceiling 3000 ft is MVFR, 3100 ft is VFR; visibility 5 SM is MVFR).
- **Page position:** the weather page takes the first placeholder slot in the page ring (after the watch face). The
  ring becomes PFD → WX → PAGE B → SETTINGS.
- **Station picker:** four letter wheels (A–Z) on the watch, because there is no keyboard. The phone setup page from S2
  is not extended for the station.
- **Certificate:** the provider's root certificate authority is built into the firmware; when it changes, a firmware
  update is needed (documented).
- **Time:** the age of a report is judged against the watch's UTC clock from S2.
- **Display:** colours follow the existing theme; night-vision colours come with a later slice.

## Dependencies

- S2 (merged): Wi-Fi configuration, the network task, the hourly sync schedule, and the rule that the radio is off
  between transactions.
- S1 (merged): theme and layout scaling, rounded-corner safe area, the page ring.
- S0 (merged): settings storage, diagnostic console, page framework.
