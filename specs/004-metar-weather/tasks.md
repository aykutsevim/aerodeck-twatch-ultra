---

description: "Task list for METAR Weather Page (Slice S3)"
---

# Tasks: METAR Weather Page (Slice S3)

**Input**: Design documents from `/specs/004-metar-weather/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/serial_commands.md, contracts/wx_page.md,
contracts/provider_http.md, quickstart.md

**Tests**: INCLUDED. Constitution Principle V requires Unity tests for every new or changed `src/core/` module, written
before the implementation. Spec FR-018 requires ≥15 real reports (the plan raises it to ≥20 for SC-001).

**Organization**:
- US1: weather at a glance (P1)
- US2: stale / failure / cache (P1)
- US3: refresh now (P2)
- US4: change the station (P2)
- US5: wind compass and raw-text layout (P3)

## Format: `[ID] [P?] [Story] Description`

## Conventions (apply to every task)

- **Running PlatformIO:** `& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio …`. For native
  tests, put
  `%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin`
  on PATH. Serial tools open COM3 with **DTR/RTS low**.
- **S0–S2 rules hold:**
  - vendor/board code only in `src/board/`;
  - LVGL only on the UI task;
  - `src/core/` is pure C++ (no Arduino/IDF headers);
  - sizes via `Theme::S()`;
  - the layout stays inside the 64 px rounded corners;
  - Wi-Fi/TLS headers only in `src/services/` (ADR-012).
- **All network/TLS work runs on the `net` task (core 0)** inside a `RailLease(Rail::Wifi)`. UI and svc code only call
  `NetService::requestWeather()` and read `Snapshot<WeatherSnapshot>`.
- **Never change or print the user's Wi-Fi credentials** during validation. Tests that need "no network" use an
  unreachable WAN or a wrong station, not credential changes, unless the user agrees.
- **References:**
  - contracts: `contracts/serial_commands.md`, `contracts/wx_page.md`, `contracts/provider_http.md`;
  - entities: `data-model.md`;
  - decisions: `research.md` R1–R10.

---

## Phase 1: Setup

- [X] T001 Create test folders `test/test_metar/` and `test/test_http_resp/`
- [X] T002 Verify the baseline on branch `004-metar-weather`: `pio test -e native` (115/115), `pio run -e twatch_ultra`
  and `pio run -e twatchs3` succeed
- [X] T003 Collect the test corpus with a PC script (scratchpad, not committed):
  - fetch `format=raw` and `format=json` from `https://aviationweather.gov/api/data/metar` for ≥20 stations;
  - choose them to cover research R6:
    - US units with fractions: KSFO, KJFK, KORD, KDEN, KBOS;
    - CAVOK: LTAI, LEMD, LGAV;
    - negative temperatures / MPS: UUEE, UNNT, ZBAA, PANC, CYYZ, ENGM;
    - VV / fog, gusts, VRB, variable sector, AUTO, NSC/NCD;
    - LTAC, LTFM.
  - Use `hours=24` to find rarer groups.
  - Keep each raw line together with the provider's `fltCat`, `wdir`, `wspd`, `wgst`, `visib`, `altim`, `temp` and
    `dewp` as the oracle.
  - Add hand-written reports for groups that can't be found live: `NIL`, `////` visibility, `/////KT`, `M1/4SM`,
    `P6SM`, `///015` clouds, and a COR.
  - Record the collection date and the station list in `specs/004-metar-weather/validation.md` (create the file).

---

## Phase 2: Foundational (blocking)

**Purpose:** the pure decoder and HTTP parser, settings v4 with the blob API, theme tokens, the page id, and the
WeatherService/NetService skeleton that every story uses.

### Tests (write first)

- [X] T004 [P] Write `test/test_metar/test_main.cpp` for `core::Metar` (data-model §1, research R6):
  - **Corpus test:** for each T003 report, assert the station, day/hour/minute, wind dir/speed/gust/sector,
    `visM`/`visText`/`cavok`, clouds (cover, base, CB), ceiling, temp/dew, QNH and category. The category must equal
    the provider's `fltCat` (document any deliberate difference inline).
  - **Wind:**
    - `27015G25KT` → 270/15/G25;
    - `VRB03KT` → VRB;
    - `27006KT 250V310` → sector 250–310;
    - `00000KT` → calm;
    - `05008MPS` → 16 kt;
    - `/////KT` → missing;
    - `36099KT` speed accepted and `36300KT` rejected (range).
  - **Visibility:**
    - `9999` → 10000 m `"10 km+"`;
    - `0800` → 800 m;
    - `CAVOK`;
    - `1 1/2SM` (two tokens) → 2414 m `"1 1/2 SM"`;
    - `M1/4SM` → `"<1/4 SM"`;
    - `P6SM` → `"6+ SM"`;
    - `10SM`;
    - `////` → missing;
    - `4000 1500NE` → prevailing 4000.
  - **Clouds and ceiling:**
    - `FEW016 BKN030` → ceiling 3000;
    - `SCT040 OVC008` → ceiling 800;
    - `VV002` → ceiling 200;
    - `NSC`/`SKC`/`CLR`/`NCD`/CAVOK → no ceiling;
    - `BKN020CB`;
    - `///015` → base unknown;
    - no cloud group and no CAVOK → ceiling unknown.
  - **Temperature, pressure, report kind:**
    - `M05/M10` → −5/−10;
    - `10/M01`;
    - `//` missing;
    - `Q1020`;
    - `A2992` → 1013;
    - `A3012` → 1020;
    - `Q0999`;
    - `AUTO`, `COR`, `SPECI` handled;
    - `METAR LTAC 111020Z NIL` → `nil`;
    - groups after `RMK`/`NOSIG`/`TEMPO`/`BECMG` are ignored.
  - **Category boundaries (FR-007):**
    - ceiling 3000 → MVFR and 3100 → VFR;
    - 1000 → MVFR, 999 → IFR;
    - 500 → IFR, 499 → LIFR;
    - visibility 5 SM → MVFR, 6 SM → VFR;
    - 3 SM → MVFR, 2 SM → IFR;
    - 1 SM → IFR, 3/4 SM → LIFR;
    - metric 8000 → MVFR, 9000 → VFR, 4800 → MVFR, 4700 → IFR, 1600 → IFR, 1500 → LIFR;
    - the worse of ceiling and visibility wins;
    - missing visibility → Unknown;
    - no cloud info (no cloud group, no CAVOK/NSC/NCD/SKC/CLR) → Unknown, except visibility < 1 SM / 1600 m → LIFR;
      visibility 2 SM with no cloud info → Unknown (not IFR: a low ceiling could make it LIFR);
    - `FEW020 SCT040` (no BKN/OVC/VV) with 10 SM → VFR (no ceiling).
  - **`resolveObsTime`:**
    - same day;
    - the previous day across midnight;
    - day 31 seen on Dec 1 → Oct/Dec boundaries correct;
    - year rollover (`312350Z` at 2027-01-01T00:05Z);
    - an obs 5 min in the future (allowed) and 20 min in the future (previous month);
    - `nowUtc` = 0 → 0.
  - **`isStale`:** 89:59 → false, 90:01 → true, obs 0 → true, now 0 → true.
  - **Format helpers:** `formatWind` (`"270° 15G25 kt"`, `"VRB 03 kt"`, `"VRB 250-310° 06 kt"`, `"CALM"`, `"--"`) and
    `formatClouds`.
  - **`pickNewest`:** a multi-line body picks the first line with a matching station; CRLF is trimmed; a line over 255
    chars is truncated safely; a body without a matching station → false.
  - **Robustness:** empty string, only spaces, 255 × `"A"`, random bytes, and a token of 40 digits all return without
    reading past the end. Fields stay missing.
- [X] T005 [P] Write `test/test_http_resp/test_main.cpp` for `core::HttpResp` (data-model §2, contracts/provider_http.md):
  - the observed 200 response (headers + `Content-Length: 63` + body) → status 200, body correct;
  - a `204 No Content` with no body;
  - `Transfer-Encoding: chunked` → `chunked=true`;
  - `Content-Length: 99999` → reject;
  - a header block > 1536 B → reject;
  - a partial buffer (headers not ended) → `complete=false`;
  - a body shorter than `Content-Length` → `complete=false` unless the closed flag is set;
  - LF-only line endings accepted;
  - a garbage status line → reject;
  - `HTTP/1.1 503 Service Unavailable` → 503.
- [X] T006 [P] Extend `test/test_settings/test_main.cpp` for schema v4:
  - `kSchemaVersion == 4`;
  - `WxStation` is `Str`, editable, not secret, with `sdef == "LTAC"`;
  - `validateString(WxStation, …)` accepts `LTAC`/`ltfm` (normalised uppercase) and rejects `LTA`, `LTACX`, `LT1C`,
    `""`, `"LT C"`;
  - migration 3→4 is additive (no key removed, the `wxstn` default is persisted).

### Implementation

- [X] T007 Implement `src/core/Metar.h/.cpp` (data-model §1, research R5/R6) so T004 passes. No heap allocation; input
  capped at 255 chars.
- [X] T008 [P] Implement `src/core/HttpResp.h/.cpp` (data-model §2) so T005 passes
- [X] T009 Settings v4 in `src/core/SettingDefs.h/.cpp`:
  - add `WxStation` (`wxstn`, Str 4..4, editable);
  - add `const char* sdef` to `SettingDef` (`nullptr` for existing keys);
  - add uppercase normalisation and validation in `validateString`;
  - `kSchemaVersion = 4`.

  Then in `src/services/SettingsStore.h/.cpp`:
  - use `sdef` for string defaults (load, restore-defaults, migration "key added → persist default");
  - add `getBlob(key, buf, cap) -> size_t`, `setBlob(key, data, len) -> bool` and `eraseBlob(key)` in namespace
    `aerodeck`, under the existing lock, with blob keys excluded from `ad set list`;
  - restore-defaults also erases `wxcache`.

  T006 must pass.
- [X] T010 [P] Theme tokens in `src/ui/Theme.h/.cpp`: `catVfr()` `#00C000`, `catMvfr()` `#3A7BFF`, `catIfr()` `#FF3030`,
  `catLifr()` `#FF40FF`, `catUnknown()` = `dim()`, plus `catColor(core::Metar::Category)`
- [X] T011 [P] Page id in `src/core/Types.h`: rename `PageId::PlaceholderA` → `PageId::Weather` (name `"WX"` wherever
  page names are printed, e.g. `ad nav`, nav logs). Update the users (`src/app/main.cpp`, `src/ui/PageManager.cpp`).
  Grep `test/` for `PlaceholderA` and `"PAGE A"` and update any native test that asserts page ids or names (the S0
  page-ring tests).
- [X] T012 [P] `src/services/WxCerts.h`: a PEM bundle with **DigiCert Global Root G2** and **DigiCert Global Root CA**,
  copied from the PlatformIO Python `certifi` `cacert.pem`. The header comment records each subject, SHA-256
  fingerprint and expiry (research R2).
- [X] T013 WeatherService skeleton in `src/services/WeatherService.h/.cpp`:
  - the `WeatherSnapshot` struct (data-model §3) and `WxBusy`/`WxResult` enums with `resultName()`;
  - `begin()`:
    - install the **size-based** mbedTLS allocator once via `mbedtls_platform_set_calloc_free` (research R3), with
      blocks ≥4096 B in PSRAM (internal fallback) and smaller blocks internal, freed with `heap_caps_free`. It must run
      before `NetService::begin()` and is never swapped;
    - load the cache;
    - publish;
  - `status()`;
  - `fetchOnNet(uint32_t deadlineMs)`, declared for net-task use only; stubbed here and filled in by T015;
  - `onStationChanged()`.

  Add `EventType::WeatherUpdated` to `src/services/EventBus.h`.
- [X] T014 NetService integration in `src/services/NetService.h/.cpp` (research R4, data-model §6):
  - `NetBusy::Weather` and `Cmd::Weather`;
  - `requestWeather()` coalesced with an atomic pending flag; it returns `false` when not configured, so callers can
    show `NO WIFI`;
  - `Cmd::Sync` calls `WeatherService::fetchOnNet(millis() + 15000)` after the NTP step whenever the STA connect
    succeeded (even if NTP failed), inside the same `RailLease`, and clears the pending flag;
  - `Cmd::Weather`:
    - on dequeue, atomically test-and-clear the pending flag; **skip** if it was already cleared (a Sync fetched
      meanwhile; research R4);
    - otherwise: lease → `connectSta(deadline)` → `fetchOnNet(deadline)` → release, with **one shared deadline =
      start + 15 s** (spec FR-003);
  - a weather failure never changes `NetStatus.lastResult` or the sync schedule (FR-004).

  Call `WeatherService::begin()` in `src/app/main.cpp` before `NetService::begin()`.

**Checkpoint:** `pio test -e native` all green, and both firmware targets build.

---

## Phase 3: User Story 1 - See my home airport's weather at a glance (P1) 🎯 MVP

**Goal:** the hourly/boot sync fetches the station's METAR over verified HTTPS. The WX page shows the decoded fields,
the category, and the raw text.

**Independent Test:** quickstart Q3 + Q4. After a boot, the WX page fields match the provider's raw text.

- [X] T015 [US1] Implement `WeatherService::fetchOnNet()` in `src/services/WeatherService.cpp` (contracts/provider_http.md,
  research R1/R2/R10):
  - `WiFiClientSecure` + `setCACert(kWxRootCerts)`, connect to `aviationweather.gov:443` with the remaining deadline;
  - write the request (station = the current `wxstn`);
  - read into a 2 KB static buffer until `HttpResp` says complete, or close/deadline;
  - map the outcome to `WxResult`;
  - on 200: `Metar::pickNewest` → `decode` → `resolveObsTime(TimeService UTC)`;
  - publish the snapshot (busy `Fetching` → `Idle`) and post `WeatherUpdated`;
  - log `[wx] fetch: <result> obs=HHMMZ cat=… ms=…`, with a TLS error code on failure;
  - log the raw line once per new observation;
  - discard the result if `wxstn` changed during the fetch (data-model §3).
- [X] T016 [US1] Serial commands in `src/services/WeatherService.cpp` per contracts/serial_commands.md: `ad wx`,
  `ad wx fields`, `ad wx fetch`, `ad wx decode "<text>" [show]`
- [X] T017 [US1] **Checkpoint on the device:**
  - flash;
  - `ad wx fetch` → `[wx] fetch: Ok` with LTAC fields matching the provider;
  - `ad stat` heapmin during 5 fetches ≥40 KB (R3); record the fetch `ms` and the handshake behaviour with PSRAM;
  - certificate self-test: a temporary build with `-DWX_CERT_SELFTEST` (a wrong root) → `NetFail tls`; then remove
    the flag;
  - `ad reboot` → `[net] sync: Ok` then `[wx] fetch: Ok`, then `rail wifi OFF` (one session).

  If heapmin <40 KB or the handshake is unstable, apply the R3 fallback and re-measure. Record everything in
  `validation.md`.
- [X] T018 [US1] Implement `src/pages/WxPage.h/.cpp` (contracts/wx_page.md, research R9):
  - header (station, `HHMMZ`, category pill in `Theme::catColor`);
  - tiles WIND/VIS/QNH/T/DP/CLOUDS using `formatWind`/`formatClouds`/`visText` (`--` when missing);
  - the raw text label (wrapping);
  - all sizes via `S()`, inside the rounded safe area (`minTopForRounded`);
  - refresh values only when `WeatherSnapshot.seq` changes (an `lv_timer`, 500 ms poll).

  In `src/app/main.cpp`, replace `g_pageA` (PlaceholderPage) with `WxPage` so the ring is
  PFD → WX → PAGE B → SETTINGS.
- [X] T019 [US1] Validate quickstart Q2 (migration/default), Q3 (10 boots: a fresh report ≤30 s, SC-002; session length;
  `wifi OFF`) and Q4 (5 live reports via `ad wx decode` + the native corpus → SC-001 20/20). Record in `validation.md`.

**Checkpoint:** MVP. The watch shows today's LTAC weather, decoded and colour-coded.

---

## Phase 4: User Story 2 - Know when the weather is old or the fetch failed (P1)

**Goal:** STALE after 90 min, error annunciators that keep the last good report, a cache across reboots, NO DATA with a
reason.

**Independent Test:** quickstart Q6.

- [X] T020 [US2] Cache in `src/services/WeatherService.cpp` (data-model §5, research R7):
  - write `wxcache` via `SettingsStore::setBlob` after `Ok` with a changed `obsUtc`;
  - load it in `begin()` (version/length/station checks; erase if invalid) and re-decode the raw text;
  - set `fromCache=true`;
  - add `ad wx clear`.
- [X] T021 [US2] Staleness and test hook:
  - `ad wx age <s>` (RAM override of the observation age) in `src/services/WeatherService.cpp`;
  - WxPage annunciator row in `src/pages/WxPage.cpp`:
    - `STALE` when `Metar::isStale(obsUtc, ClockSnapshot utc)`, which also covers a clock that isn't set;
    - re-evaluated every 30 s while visible and on clock changes;
  - error text from `lastResult`: `NO WIFI` / `NET FAIL` / `NO REPORT` / `SRV ERROR`, cleared by the next `Ok`;
  - fetched time `upd HH:MMZ`.
- [X] T022 [US2] NO DATA state in `src/pages/WxPage.cpp`: the station + `NO DATA` + the reason when `hasReport == false`.
  `NO WIFI` comes from `NetStatus.configured == false`.
- [ ] T023 [US2] Validate quickstart Q6 steps 1, 2, 4, 5 and 7 on the device:
  - wrong station → NoReport;
  - STALE via `ad wx age`;
  - a cached report after a reboot within 5 s of the face (SC-005);
  - clock not set → STALE.

  Steps 3 (unreachable WAN ×10, SC-004) and 6 (Wi-Fi forgotten) **need the user**: ask, and do not change credentials
  without consent. Record in `validation.md`.

---

## Phase 5: User Story 3 - Refresh now (P2)

**Goal:** a REFRESH button with immediate feedback, coalesced fetches, and a smooth UI.

**Independent Test:** quickstart Q5.

- [X] T024 [US3] REFRESH button in `src/pages/WxPage.cpp`:
  - a tap sets a local `refreshing` flag (redrawn in the same LVGL cycle, ≤100 ms) and calls
    `NetService::requestWeather()`;
  - if that returns `false` (not configured), show `NO WIFI` and no spinner;
  - disabled look while `busy != Idle`; taps while busy are ignored;
  - the `REFRESHING…` text replaces `upd …` until the snapshot shows `Idle`.
- [X] T025 [US3] Validate quickstart Q5:
  - 10 refreshes with `ad att sweep on`: feedback, the result ≤20 s, one fetch per double-tap;
  - FPS median ≥28 and heapmin ≥40 KB from `ad stat` during the fetches (SC-003, FR-019);
  - a swipe during a fetch stays smooth.

  Record in `validation.md`.

---

## Phase 6: User Story 4 - Change the station (P2)

**Goal:** pick an ICAO code on the watch or over serial. It is kept across reboots and fetched immediately.

**Independent Test:** quickstart Q7.

- [X] T026 [US4] `ad wx set <ICAO>` in `src/services/WeatherService.cpp` (validates via `validateString`, uppercase). On
  `SettingsChanged(WxStation)`, WeatherService:
  - clears the snapshot report;
  - erases `wxcache`;
  - publishes `NO DATA`;
  - calls `NetService::requestWeather()`.

  `ad set wxstn` takes the same path. The handler acts on **every** `SettingsChanged(WxStation)` event, even when the
  value is unchanged. `SettingsStore::restoreDefaults()` publishes it for every key, so restore-defaults (which also
  erases `wxcache`, T009) clears the in-RAM report and re-fetches as well.
- [X] T027 [US4] Settings page row "Weather station" in `src/pages/SettingsPage.cpp`:
  - shows the current code;
  - a tap opens a modal with four `lv_roller`s (A–Z, initialised to the current letters) and OK/Cancel;
  - OK → `SettingsStore::setStr(WxStation, code)`; Cancel closes.

  Follow the S2 time-zone roller modal pattern.
- [ ] T028 [US4] Validate quickstart Q7: Settings change to LTFM under 30 s; the LTFM report ≤20 s; kept after a reboot;
  serial case-insensitivity and rejection; restore LTAC afterwards. Record in `validation.md` (SC-007).

---

## Phase 7: User Story 5 - Wind on a compass and the raw report (P3)

**Goal:** a north-up wind compass with an arrow, and the full raw text readable inside the rounded screen.

**Independent Test:** quickstart Q8.

- [X] T029 [US5] Compass widget in `src/pages/WxPage.cpp` (or `src/ui/WindCompass.h/.cpp` if over ~80 lines):
  - an `lv_arc` ring with N/E/S/W labels, north up;
  - the arrow is an `lv_line` whose points are recomputed from `windDir` (pointing from the wind's origin to the
    centre);
  - the speed in the centre;
  - `CALM` / `VRB` text (plus a sector arc when `windVarFrom/To` is set);
  - `--` when missing;
  - updated only on snapshot change.
- [X] T030 [US5] Raw text container in `src/pages/WxPage.cpp`: a vertically scrollable, wrapping label kept inside the
  rounded safe area, so no part is clipped. Check it with the longest corpus report.
- [X] T031 [US5] Validate quickstart Q8 with `ad wx decode "<text>" show`:
  - VRB with gusts and a sector;
  - fractions in SM;
  - negative temperatures;
  - CAVOK;
  - calm;
  - the longest corpus report.

  Check the compass arrow direction against 4 bearings (000/090/180/270), with no clipping. Record in
  `validation.md`.

---

## Phase 8: Polish & Cross-Cutting

- [X] T032 [P] Amend `docs/software_requirements_specification.md`:
  - new **FR-WX-006** (M): "The METAR station shall be selectable on the watch and over the diagnostic console,
    validated as a 4-letter ICAO code, and persisted (default LTAC); a change triggers a fetch";
  - FR-WX-001 note: the weather is fetched in the hourly sync radio session;
  - the S3 traceability row lists FR-WX-006.
- [X] T033 [P] Amend `docs/software_architecture_specification.md`:
  - the `WeatherService` row: raw format, `core::Metar`/`core::HttpResp`, PSRAM mbedTLS allocator, NVS `wxcache`,
    `WxResult`;
  - the `SettingsStore` row: schema v4, `wxstn`, the `wxcache` blob;
  - the `WxPage` mention.
- [X] T034 [P] Update `README.md`: the WX page (fields, category colours, STALE/errors, REFRESH), the weather station
  setting, the `ad wx` commands, and a link to `specs/004-metar-weather/contracts/serial_commands.md`
- [X] T035 Regression and portability:
  - `pio test -e native` (all suites);
  - `pio run -e twatchs3`;
  - the layering grep (`WiFiClientSecure`/`mbedtls` only in `src/services/`; no Arduino headers in `src/core/`; no
    `lv_` in services);
  - the S1 regression script with its page-ring expectations updated (`PAGE A` → `WX`): nav, PFD FPS median ≥28 with
    sweep, attitude;
  - the S2 boot sync still passes (`[net] sync: Ok` on 3 boots).
- [ ] T036 Run quickstart Q9: a 24-h soak with serial logging. Pass criteria:
  - 24/24 `[wx] fetch: Ok`;
  - `rail wifi OFF` after each;
  - no unmarked report older than 90 min (SC-006).

  It may run together with S2 T038. Then complete `specs/004-metar-weather/validation.md` with SC-001…SC-007.
- [X] T037 After the PR is merged, update `docs/product_roadmap.md` per the **status tracking rule**: S3 → ✅ Done with
  the date, the PR number and any pending manual checks; M1 → ✅ when its DoD is met.

---

## Dependencies & Execution Order

- **Setup (T001–T003)** → **Foundational (T004–T014)**:
  - tests T004–T006 come before T007–T009;
  - T003 (corpus) comes before T004;
  - T010–T012 are independent;
  - T013 comes before T014.
- **US1 (T015–T019):** needs Foundational. T015–T017 (fetch + checkpoint) come before T018 (page).
- **US2 (T020–T023):** needs T015 and T018.
- **US3 (T024–T025):** needs T014 and T018. It can run in parallel with US2, except that WxPage is a shared file, so
  sequence those edits.
- **US4 (T026–T028):** needs T009 and T015. The SettingsPage edits are independent of WxPage.
- **US5 (T029–T031):** needs T018.
- **Polish:** after the stories. T036 (24 h soak) is the long pole. T037 runs only after the merge.

### Parallel Opportunities

- T004 ∥ T005 ∥ T006 (test files).
- T008 ∥ T010 ∥ T011 ∥ T012.
- T026/T027 (station: WeatherService serial + SettingsPage) ∥ T020–T022 (cache/STALE), except the WeatherService file,
  so sequence edits there.
- T032 ∥ T033 ∥ T034.

## Parallel Example: tests first

```text
Task: "T004 Metar decoder test in test/test_metar/test_main.cpp"
Task: "T005 HttpResp test in test/test_http_resp/test_main.cpp"
Task: "T006 settings v4 test in test/test_settings/test_main.cpp"
```

## Implementation Strategy

1. **MVP = Foundational + US1.** The watch shows the current LTAC weather after every sync.
2. **US2:** trustworthiness (STALE, errors, cache). Required before merge, since both are P1.
3. **US3 then US4:** refresh and station change.
4. **US5:** the compass and raw-text polish.
5. **Docs, regression, soak, merge.** Then the roadmap update (T037), which closes milestone M1.

## Notes

- `validation.md` is created at T003 and completed at T036.
- **If an installed API differs from what a task names** (e.g. `WiFiClientSecure::setCACert` signature,
  `mbedtls_platform_set_calloc_free` availability): follow the framework source in
  `~/.platformio/packages/framework-arduinoespressif32` and note the difference in `validation.md`.
- **The weather data is public:** logging raw METAR is allowed. The Wi-Fi credentials are still never logged.
