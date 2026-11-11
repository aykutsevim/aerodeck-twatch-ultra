---

description: "Task list for Time Sync & Connectivity (Slice S2)"
---

# Tasks: Time Sync & Connectivity (Slice S2)

**Input**: Design documents from `/specs/003-time-sync/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/serial_commands.md, contracts/setup_portal.md, quickstart.md

**Tests**: INCLUDED. Constitution Principle V requires Unity tests for every new or changed `src/core/` module, written
before the implementation.

**Organization**: US1 Wi-Fi setup (P1) · US2 automatic time sync (P1) · US3 named time zones (P2) · US4 sync status /
TIME UNSYNC (P3).

## Format: `[ID] [P?] [Story] Description`

## Conventions (apply to every task)

- PlatformIO via `& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio …`. For native tests, put
  `%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin`
  on PATH. Serial tools open COM3 with **DTR/RTS low**.
- S0/S1 rules hold: vendor/board code only in `src/board/`; LVGL only on the UI task; `src/core/` pure C++; sizes via `Theme::S()`;
  the layout stays inside the 64 px rounded corners.
- **All blocking network calls run on the `net` task (core 0).** UI and svc code only post commands and read `Snapshot<NetStatus>`.
- **Secrets**: never pass `wifipsk` or the setup access code to `Log`, serial replies, or labels (show `***` / `••••`).
- Contracts: `contracts/serial_commands.md`, `contracts/setup_portal.md`. Entities: `data-model.md`. Decisions: `research.md` R1–R9.

---

## Phase 1: Setup

- [X] T001 Create test folders `test/test_ntp/`, `test/test_posix_tz/`, `test/test_zones/`, `test/test_sync_schedule/`
- [X] T002 Verify the baseline: `pio test -e native` (82/82) and `pio run -e twatch_ultra` succeed on branch `003-time-sync`

---

## Phase 2: Foundational (blocking)

**Purpose**: Settings v3 (strings, secrets, zones), quoted serial tokens, the time-zone engine (needed by the migration),
and the `net` task skeleton.

### Tests (write first)

- [X] T003 [P] Extend `test/test_command_parser/test_main.cpp`:
  - `ad wifi set "My Home WiFi" "pa ss"` → 2 args `My Home WiFi`, `pa ss`;
  - `\"` and `\\` escapes inside quotes;
  - empty quoted token `""` → empty arg;
  - an unterminated quote → `ERR unterminated quote`;
  - a 128-char line with a 32-char SSID + 63-char PSK fits.
- [X] T004 [P] Write `test/test_posix_tz/test_main.cpp` for `core::PosixTz`:
  - parse `<+03>-3` (offset +180, no DST);
  - `UTC0`;
  - `GMT0BST,M3.5.0/1,M10.5.0`: 2027-03-28T00:59:59Z → +0, 01:00:00Z → +60; 2027-10-31T00:59:59Z → +60, 01:00:00Z → +0;
  - `EST5EDT,M3.2.0,M11.1.0`: 2027-03-14 06:59:59Z → −300, 07:00:00Z → −240;
  - southern hemisphere `AEST-10AEDT,M10.1.0,M4.1.0/3`: January → +660, July → +600;
  - `<+0530>-5:30` → +330; `<+0545>-5:45` → +345; `<-0330>3:30` → −210;
  - invalid strings (`""`, `XYZ`, `GMT0BST,M13.1.0`) → parse false.
- [X] T005 [P] Write `test/test_zones/test_main.cpp` for `core::Zones`:
  - count ≥ 45 named zones;
  - every entry's POSIX string parses (loop over all with `PosixTz`);
  - `find("Istanbul")` is the default and has offset 180, no DST;
  - `find("london")` is case-insensitive;
  - `bestMatch(180)` = Istanbul, `bestMatch(330)` = Kolkata, `bestMatch(-300)` = a non-DST zone or `UTC-05:00`, `bestMatch(345)` = Kathmandu;
  - for every 30-min offset from −720 to +840, `bestMatch` returns a zone whose January and July offsets both equal it;
  - names are unique.
- [X] T006 [P] Extend `test/test_settings/test_main.cpp` for schema v3:
  - `kSchemaVersion == 3`;
  - `wifissid` Str max 32, editable=false via the generic `ad set`;
  - `wifipsk` Str max 63, secret, and `formatSettingForOutput` → `***`;
  - `tzid` U16 default = `Zones::defaultIndex()`;
  - `syncmin` 15..1440 default 60;
  - `lastsync` I32 internal;
  - `tzoff` no longer in the registry;
  - `validateString(Key, "…")` length rules (ssid 1..32; psk 8..63; an open network / empty psk is rejected).

### Implementation

- [X] T007 Implement quoted tokens in `src/core/CommandParser.cpp` (`tokenize` handles `"…"` with `\"`/`\\` escapes in place; returns −1 → dispatch replies `ERR unterminated quote`) so T003 passes. Keep every S0 test green
- [X] T008 Implement `src/core/PosixTz.h/.cpp` per data-model §3 (pure; reuse `core::ClockMath` day arithmetic for rule dates: the w-th weekday of a month, w=5 = last; rule time in local standard/DST time per POSIX) so T004 passes
- [X] T009 Implement `src/core/Zones.h/.cpp`:
  - a `constexpr` table of ≥45 named zones + fixed offsets per data-model §2. Named zones (name → POSIX), at least: Abu Dhabi, Adelaide, Amsterdam, Anchorage, Athens, Auckland, Baghdad, Bangkok, Berlin, Bogota, Brisbane, Buenos Aires, Cairo, Chatham, Chicago, Darwin, Denver, Dhaka, Dubai, Hawaii, Hong Kong, Istanbul, Jakarta, Johannesburg, Kabul, Karachi, Kathmandu, Kolkata, Lisbon, London, Los Angeles, Madrid, Mexico City, Moscow, Mumbai, Nairobi, New York, Newfoundland, Paris, Phoenix, Reykjavik, Riyadh, Santiago, Sao Paulo, Seoul, Singapore, Sydney, Tehran, Tokyo, Yangon;
  - POSIX strings from the IANA tzdata 2025 `posix_tz_db` mapping (record the version in a header comment);
  - functions `count()`, `at(i)`, `find(name)`, `bestMatch(offsetMin)`, `defaultIndex()`.

  T005 must pass.
- [X] T010 Extend `src/core/SettingDefs.h/.cpp` for schema 3:
  - add `SettingType::Str` and `validateString`;
  - keys `WifiSsid`, `WifiPsk` (secret), `TzId`, `SyncMin`, `LastSync`; remove `TzOff`;
  - `kSchemaVersion = 3`;
  - add `migrateTzoffToTzid(int32_t tzoff) → Zones::bestMatch`.

  T006 must pass. Update `test_settings` S1 cases that referenced `TzOff` (move them to `tzid`).
- [X] T011 Extend `src/services/SettingsStore.h/.cpp`:
  - string values (`getStr(key, char* out, size_t n)`, `setStr(key, const char*, bool internal)`), using fixed buffers in the cache, `Preferences::putString/getString`, written on the svc task like the numbers;
  - migration 2→3: read `tzoff` (if present) → `tzid = migrateTzoffToTzid(tzoff)`, `remove("tzoff")`, write defaults for the new keys;
  - `ad set list` prints strings, with `wifipsk=***`.
- [X] T012 Replace the S1 `tzoff` usage: `src/services/TimeService.cpp` derives `tzOffsetMin` from `PosixTz(Zones::at(tzid).posix).offsetMinutes(now)` every tick (1 Hz); `src/pages/SettingsPage.cpp` temporarily shows the zone name (the roller comes in US3). Remove every `SettingKey::TzOff` reference
- [X] T013 Create `src/services/NetService.h/.cpp` skeleton (research R1):
  - a `net` FreeRTOS task on core 0 (prio 1, 8192-byte stack) waiting on a command queue (`Sync`, `Test`, `Scan`, `SetupStart`, `SetupStop`);
  - Wi-Fi power through **`PowerService` (constitution III)**: add `Rail::Wifi` to `src/core/Types.h`, add `PowerService::setRailHook(Rail, void(*)(bool on))`, used instead of `board().setRail` for rails that have a hook. `NetService::begin` registers the hook (`false` → `WiFi.disconnect(true); WiFi.mode(WIFI_OFF)`). `PowerService::begin` forces `Rail::Wifi` OFF at boot. Every transaction holds a `RailLease(Rail::Wifi)` taken and released on the net task; logs `rail wifi ON/OFF`;
  - `Snapshot<NetStatus>` per data-model §4;
  - `WiFi.onEvent` handler capturing the disconnect reason;
  - publish `EventType::NetStatusChanged` (add it to `src/services/EventBus.h`);
  - start the task in `src/app/main.cpp` after `TimeService::begin()`.
- [X] T014 Build, flash, check boot: settings migrate (log shows `migration added`, `ad set list` has `tzid`, no `tzoff`), the PFD LOCAL time is unchanged (Istanbul), `pio test -e native` is all green. Record in `specs/003-time-sync/validation.md`

**Checkpoint**: Foundation ready. Time display uses zones; the net task idles with Wi-Fi off.

---

## Phase 3: User Story 1 - Connect the watch to my Wi-Fi (P1) 🎯 MVP (with US2)

**Goal**: Credentials via USB command and phone setup hotspot, test connection with a classified result, forget, masked everywhere.

**Independent Test**: quickstart Q3 (USB part) + Q4 (phone hotspot).

- [X] T015 [US1] Implement connect + test in `src/services/NetService.cpp`:
  - `connectSta(ssid, psk, timeoutMs=15000)` → `Ok | WrongPassword | NotFound | Timeout` (from the event reason: `WIFI_REASON_AUTH_FAIL`/`AUTH_EXPIRE`/`4WAY_HANDSHAKE_TIMEOUT` → WrongPassword, `NO_AP_FOUND` → NotFound);
  - the `Test` command does connect + one NTP query (from US2's `core::Ntp`; until then connect only) without changing the clock, and logs `[net] test: <result>`;
  - always release the `RailLease(Rail::Wifi)` (RAII scope inside the command handler).
- [X] T016 [US1] Serial commands in `src/services/NetService.cpp` per `contracts/serial_commands.md`: `ad wifi`, `ad wifi set "<ssid>" "<psk>"` (validate lengths, `SettingsStore::setStr(...internal)`, then post `Test`), `ad wifi forget`, `ad wifi test`, `ad wifi scan` (Scan command: `WiFi.scanNetworks`, dedupe by SSID keeping the strongest, sort by RSSI, ≤20). Replies never contain the PSK
- [ ] T017 [US1] Checkpoint on the device: send `ad wifi set` (other credentials) while an `ad sync` is running → the running attempt completes or times out, and the next attempt uses the new credentials (spec edge case); `ad wifi set` with the real network → `test: Ok`; wrong password → `WrongPassword` ≤20 s; `ad wifi` → `psk=*** wifi=OFF`; reboot keeps credentials; `ad wifi forget` clears them. Record in `validation.md`
- [X] T018 [US1] Implement `src/services/SetupPortal.h/.cpp` per `contracts/setup_portal.md` (executes inside the net task):
  - scan → `WiFi.mode(WIFI_AP_STA)`, `softAP("AeroDeck-XXXX", code)` where the code is 8 chars from `esp_random()` over the alphabet `ABCDEFGHJKMNPQRSTUVWXYZ23456789`;
  - `DNSServer` on port 53 answering `*` with 192.168.4.1;
  - `WebServer` on port 80 with the routes `/`, `/scan.json`, `/save`, the portal probes and onNotFound→302;
  - the embedded page (`static const char kPage[] PROGMEM`, ≤6 KB, plain HTML/JS: fetch `/scan.json`, clickable list, SSID/password fields, Save);
  - a loop calling `dns.processNextRequest()` + `server.handleClient()` with a 300 s deadline and a stop flag;
  - on valid save: persist (via `SettingsStore::setStr`), stop the AP and DNS, then run the `Test` flow;
  - expose `setupSsid`, `setupCode`, `setupRemainingS` in `NetStatus`;
  - never log the code;
  - serial `ad wifi setup` (reply has the SSID only) / `ad wifi setup stop`.
- [X] T019 [US1] Settings page Wi-Fi section in `src/pages/SettingsPage.cpp`:
  - rows "Network" (SSID or "not set"), "Status" (last result), buttons **Set up Wi-Fi** (→ `NetService::startSetup()`) and **Forget** (confirm msgbox);
  - a **setup modal** (`lv_msgbox` or a full-screen overlay on `lv_layer_top`) with `lv_qrcode` (`WIFI:T:WPA;S:<ssid>;P:<code>;;`), the SSID, the code in large text, `http://192.168.4.1`, an mm:ss countdown and Cancel;
  - refresh on `NetStatusChanged`;
  - keep everything inside the rounded-corner safe area.
- [ ] T020 [US1] Validate quickstart Q4 on **both an iOS and an Android phone** (FR-004c), and time one first-time setup from "Set up Wi-Fi" to time synced (SC-007, ≤3 min): QR join, page loads automatically, network list, save → watch shows Connected; cancel and 5-min expiry close the AP (`ad wifi` → `wifi=OFF`); the serial log contains no code or PSK. Record in `validation.md`

**Checkpoint**: The watch can be put on a network without a computer.

---

## Phase 4: User Story 2 - Accurate time without thinking about it (P1)

**Goal**: SNTP sync at boot and every `syncmin`, RTC written, Wi-Fi off after, Sync now, implausible times rejected.

**Independent Test**: quickstart Q3 (sync part) + Q5.

### Tests (write first)

- [X] T021 [P] [US2] Write `test/test_ntp/test_main.cpp` for `core::Ntp`:
  - `buildRequest` byte 0 = 0x23, transmit timestamp encoded big-endian;
  - `parseResponse` rejects mode≠4, stratum 0 and >15, LI=3, an originate mismatch, transmit 0;
  - a correct response yields UTC seconds/ms (vector: server transmit 2026-11-11T09:24:35.250Z, symmetric delay → offset);
  - NTP era conversion (`ntpToUnix(3_000_000_000)`).
- [X] T022 [P] [US2] Write `test/test_sync_schedule/test_main.cpp` for `core::SyncSchedule`:
  - boot with/without credentials;
  - success → next at +syncmin;
  - failure backoff 2/5/15 min capped at syncmin;
  - `onConfigured` → due now;
  - `isUnsynced` (never synced & configured → true; 23 h → false; 25 h → true; not configured → false).

### Implementation

- [X] T023 [US2] Implement `src/core/Ntp.h/.cpp` (data-model §7) and `src/core/SyncSchedule.h/.cpp` (data-model §5) so T021/T022 pass
- [X] T024 [US2] Implement the `Sync` command in `src/services/NetService.cpp`:
  - lease → `connectSta` → for each server in `{pool.ntp.org, time.google.com, time.cloudflare.com}`: `WiFiUDP` send `Ntp::buildRequest`, wait ≤3 s, `Ntp::parseResponse`;
  - on success, if `clockPlausible` → `TimeService::set(utc)`, persist `lastsync`, log `[net] sync: Ok offset_ms=… server=…`; otherwise `Rejected`;
  - total ≤15 s; release the lease; update `NetStatus` + `SyncSchedule`.
  - Make the US1 `Test` command use the same NTP query without `set`.
- [X] T025 [US2] Scheduling in `src/services/NetService.cpp`: a svc tick (1 Hz) asks `SyncSchedule` whether a sync is due and posts `Sync` (at boot if configured; after `ad wifi set`/setup success → `onConfigured`). Serial `ad sync`. Settings page **Sync now** button (posts `Sync`, shows "Syncing…" then the result). Setting `syncmin` editable via `ad set syncmin`
- [ ] T026 [US2] Validate quickstart Q3 (boot sync ×10 vs PC UTC ±1 s, CLOCK NOT SET cleared, `wifi=OFF` between) and Q5:
  - an absent SSID → 10× ≤15 s;
  - `ad att sweep on` FPS ≥28 during syncs;
  - `ad stat` heapmin before/during/after ≥40 KB. If heap <40 KB, apply plan note 5 and re-measure.
  Record in `validation.md`

**Checkpoint**: MVP: the watch keeps accurate time by itself.

---

## Phase 5: User Story 3 - Local time with real time-zone rules (P2)

**Goal**: Pick a named zone on the watch or via serial; DST is honoured; ZULU unaffected.

**Independent Test**: quickstart Q2 + Q6.

- [X] T027 [US3] Serial `ad tz`, `ad tz list [filter]`, `ad tz set <name|index>` in `src/services/TimeService.cpp` per contract (`tz=<name> posix=<…> offset_min=… dst=0|1`)
- [X] T028 [US3] Settings page "Time zone" row in `src/pages/SettingsPage.cpp`: shows the zone name; tap opens a modal `lv_roller` with all zones (named first, then fixed), selected = current; OK applies via `SettingsStore::set(TzId, i)`; Cancel closes. Remove the S1 "Local offset" row
- [X] T029 [US3] Ensure the PFD LOCAL time updates ≤1 s after a zone change and at DST instants: `TimeService` refreshes `ClockSnapshot` on `SettingsChanged(TzId)` immediately (not only on the 1 Hz tick), and the PFD minute key already includes `tzOffsetMin` (S1). Verify in `src/pages/PfdPage.cpp`
- [X] T030 [US3] Validate quickstart Q2 (migration preserved LOCAL) and Q6 (London spring/autumn changeover at the exact second, Kolkata +5:30, ZULU unaffected). Record in `validation.md`

---

## Phase 6: User Story 4 - Know when time might be stale (P3)

**Goal**: "Last sync" age on Settings; TIME UNSYNC advisory after 24 h.

**Independent Test**: quickstart Q7.

- [X] T031 [US4] Add `CautionId::TimeUnsync` "TIME UNSYNC" to `src/services/Cautions.h`. In the NetService svc tick, raise/clear via `SyncSchedule::isUnsynced(nowUtc, lastsync, configured)`. Test hook `ad wifi age <s>` overrides the effective last-sync age (RAM only)
- [X] T032 [US4] Settings page row "Last sync" in `src/pages/SettingsPage.cpp` ("never", "N min ago", "N h ago"), refreshed every 10 s while the page is visible and on `NetStatusChanged`
- [X] T033 [US4] Validate quickstart Q7. Record in `validation.md`

---

## Phase 7: Polish & Cross-Cutting

- [X] T034 [P] Amend `docs/software_requirements_specification.md`:
  - FR-TIM-003 → named zones (built-in table, POSIX rules), interim note removed;
  - new **FR-NET-003** (M): "The phone setup hotspot starts only on a deliberate action, is protected by a one-time code shown only on the watch, closes after ≤5 min or on save/cancel, and never overlaps a sync";
  - FR-TIM-007 → "fallback when no network";
  - the S2 traceability row lists FR-NET-003.
- [X] T035 [P] Write `docs/adr/012-wifi-stack-platform-service.md` (context: constitution II "hardware access goes through IBoard" vs the SoC Wi-Fi stack; decision: `WiFi`/`WiFiUDP`/`WebServer`/`DNSServer` allowed only in `src/services/` as a platform service identical on all boards, with power still via `PowerService` `Rail::Wifi`; consequences; alternative: wrap in `IBoard`). Amend `docs/software_architecture_specification.md` §6 (NetService: `net` task, `Rail::Wifi` lease, SetupPortal), §7.1 (settings v3 keys, unencrypted NVS note) and add the ADR-012 row to §11
- [X] T036 [P] Update `README.md`: Wi-Fi setup (phone hotspot steps + `ad wifi set`), automatic time, time zones, the new serial commands
- [X] T037 Regression and portability:
  - `pio test -e native` (all suites);
  - `pio run -e twatchs3`;
  - the layering grep (ADR-012: `WiFi.h`/`WiFiUdp.h`/`WebServer.h`/`DNSServer.h` only in `src/services/`, never in `core/`, `ui/`, `pages/`; no `lv_` in services);
  - a **secrets grep**: no `LOG*`/`Log::`/`r.line` call takes `wifipsk`/the PSK buffer/the setup code (inspect `NetService.cpp`, `SetupPortal.cpp`, `SettingsStore.cpp`);
  - the S0 `ad nav` and S1 PFD checks still pass.
- [ ] T038 Run quickstart Q8 (24-h soak with serial logging: 24/24 syncs Ok, `wifi OFF` after each, a PSK grep of the log → 0) and complete `specs/003-time-sync/validation.md` with SC-001…SC-008
- [X] T039 After the PR is merged: update `docs/product_roadmap.md` per the **status tracking rule** (S2 → ✅ Done with date + PR number + pending manual checks; M1 status)

---

## Dependencies & Execution Order

- **Setup (T001–T002)** → **Foundational (T003–T014)**: tests T003–T006 before T007–T011. T009 (Zones) before T010
  (migration uses `bestMatch`). T013 is independent of T007–T012.
- **US1 (T015–T020)** needs Foundational. T015–T017 (USB path) before T018–T020 (hotspot).
- **US2 (T021–T026)** needs T013 + T015 (`connectSta`). Tests T021/T022 can be written in parallel with US1.
- **US3 (T027–T030)** needs Foundational only (T008/T009/T012). It can run in parallel with US1/US2 (different files, except
  SettingsPage, which is shared, so sequence those edits).
- **US4 (T031–T033)** needs US2 (`lastsync`, SyncSchedule).
- **Polish** after the stories. T038 (24 h soak) is the long pole. T039 runs only after merge.

### Parallel Opportunities

- T003 ∥ T004 ∥ T005 ∥ T006 (test files).
- T008 ∥ T013 (core vs service).
- T021 ∥ T022, and both ∥ US1 implementation.
- T027 (TimeService) ∥ T018 (SetupPortal).
- T034 ∥ T035 ∥ T036.

## Parallel Example: tests first

```text
Task: "T003 quoted tokens test in test/test_command_parser/test_main.cpp"
Task: "T004 PosixTz test in test/test_posix_tz/test_main.cpp"
Task: "T005 Zones test in test/test_zones/test_main.cpp"
Task: "T006 settings v3 test in test/test_settings/test_main.cpp"
```

## Implementation Strategy

1. **MVP = Foundational + US1 USB path (T015–T017) + US2.** The watch keeps accurate time over Wi-Fi configured from a computer.
2. Then the US1 phone hotspot (T018–T020): no computer needed.
3. US3 zones, US4 status, polish, then the 24 h soak and merge. Then the roadmap update (T039).

## Notes

- `validation.md` is created at T014 and completed at T038.
- If an installed API differs from what a task names (e.g. `WIFI_REASON_*` names, `WebServer` method names), follow the
  framework source in `~/.platformio/packages/framework-arduinoespressif32` and note it in `validation.md`.
- Never print or display the PSK or the setup code. Use `***` in text and `••••` on the watch.
