# Research: Time Sync & Connectivity (S2)

Verified against the installed Arduino-ESP32 2.0.17 framework (`framework-arduinoespressif32`), LilyGoLib 0.2.0's
`lv_conf.h`, the esp32s3 `qio_qspi` sdkconfig, and S0/S1 measurements. Date 2026-11-11.

---

## R1. Network stack and where it runs (FR-007, FR-008)

**Findings**:
- Built-in libraries are available with no new dependency: `WiFi`, `WebServer` (synchronous), `DNSServer`, `Preferences`.
- sdkconfig: `CONFIG_ESP32_WIFI_TASK_PINNED_TO_CORE_0=1` (the Wi-Fi driver task runs on core 0, away from the UI on
  core 1); `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=1` (Wi-Fi/LWIP buffers prefer PSRAM); 8 static + 32 dynamic RX buffers.
- `WiFi.mode(WIFI_OFF)` calls `espWiFiStop()`, which deinitialises the driver (`WiFiGeneric.cpp:707/728`).

**Decision**: A dedicated **`net` FreeRTOS task on core 0** (priority 1, 8 KB stack) runs every blocking network
operation (connect, scan, NTP, setup portal). It is started on demand and idles on a command queue. Other modules only
post commands (`NetService::requestSync()`, `startSetup()`, `testCredentials()`) and read `Snapshot<NetStatus>`. The UI
never waits.
- **Wi-Fi lease (constitution III)**: Wi-Fi is switched **only through `PowerService`**:
  - a new `core::Rail::Wifi` is added;
  - `NetService` registers a switch hook with `PowerService` (`setRailHook(Rail::Wifi, fn)`); `fn(false)` runs
    `WiFi.disconnect(true); WiFi.mode(WIFI_OFF)`, and `fn(true)` lets the next mode change start the driver;
  - every transaction and the setup session hold a `RailLease(Rail::Wifi)`. The 0↔1 refcount transitions call the hook
    and log `rail wifi ON/OFF`, like the PMU rails. Wi-Fi is forced OFF at boot together with the other optional rails.
  - Because the hook runs on the caller's task, every lease is taken and released on the `net` task.
- **Mutual exclusion** (FR-004a): sync and setup never overlap. Commands are serialised by the single `net` task.

**Risk and measurement**: Wi-Fi interrupt and CPU load on core 0 and its effect on PFD FPS (core 1), and the internal-heap
dip during a session (Wi-Fi needs internal RAM for its static RX buffers). Both are measured in the quickstart (FPS
during a sync with `ad att sweep on`; `ad stat` heapmin before/after; the gate is heap ≥40 KB and FPS not lower than the
S1 median of 30 by more than 2).

---

## R2. Time source: minimal SNTP client (FR-005, FR-009)

**Decision**: An own minimal SNTP (RFC 4330) query over `WiFiUDP`. The request/response codec is a pure
`core::Ntp` module, host-tested:
- request: 48 bytes, LI=0, VN=4, Mode=3, with the transmit timestamp set;
- response validation: Mode=4, Stratum 1–15, non-zero transmit timestamp, LI≠3, and the originate timestamp equal to
  our transmit timestamp (anti-spoof/mismatch check);
- the clock offset uses the standard four-timestamp formula, and the result is rejected if `!clockPlausible(year)`
  (FR-009).
- Servers tried in order, 3 s each: `pool.ntp.org`, `time.google.com`, `time.cloudflare.com`. The whole transaction
  is ≤15 s, including the connection.
- The result is applied with `TimeService::set(utc)` (writes the RTC + system time; clears CLOCK NOT SET, from S1).

**Rationale**: `configTime()`/SNTP runs in the background, with less control over timeouts, validation and one-shot
behaviour. A 48-byte packet codec is trivial and fully unit-testable.

**Alternatives**: the ESP-IDF `esp_sntp` component (background polling, harder to bound); HTTP `Date` headers (less precise).

---

## R3. Time zones with summer-time rules (FR-013–FR-016)

**Decision**:
- **`core::PosixTz`** (pure, host-tested) parses POSIX TZ strings, e.g. `GMT0BST,M3.5.0/1,M10.5.0`,
  `<+0530>-5:30`, `EST5EDT,M3.2.0,M11.1.0`. It computes `offsetMinutes(utcSec)` and `isDst(utcSec)` with the
  `Mm.w.d[/time]` rule form. That's the only form current IANA zones need, including negative and >24 h times.
- **`core::Zones`**: a built-in, sorted table of **45 named zones** (display name + POSIX string), covering every UTC offset
  in use (−12…+14, including +3:30, +4:30, +5:30, +5:45, +6:30, +8:45, +9:30, +10:30, +12:45). There are also **55 fixed-offset
  entries** `UTC−12:00 … UTC+14:00` in 30-min steps (plus :45 where used). They guarantee the S1 migration always finds an
  exact match (FR-015).
- The setting `tzid` (U16, index into the table, default = "Istanbul") **replaces** `tzoff`.
- Display code calls `core::localFromUtc(utc, zone)`. `ClockMath::splitLocal` takes an offset that `PosixTz` computes
  per frame. That's cheap: the rule is evaluated once per minute and cached in `ClockSnapshot.tzOffsetMin`.
- The source of the POSIX strings is the IANA tzdata 2025 release, as published in the widely used `posix_tz_db` mapping. The table lists
  the tzdata version it was taken from.

**Rationale**: Host-testable DST transitions (SC-006). There's no dependency on newlib's `setenv("TZ")`/`localtime_r`,
which keeps global state and is untestable on the host.

**Alternatives**: storing a raw POSIX string as the setting (hard to pick on a watch); full tzdata (far too big).

---

## R4. Settings schema v3: strings and secrets (FR-001, FR-002)

**Decision**:
- `SettingType::Str` is added (max length in `max`). Stored with `Preferences::putString/getString`. The cache uses fixed
  char buffers (no heap churn).
- New keys:
  - `wifissid` (Str ≤32, editable via setup/serial);
  - `wifipsk` (Str ≤63, **secret**);
  - `tzid` (U16);
  - `syncmin` (U16, 15–1440, default 60);
  - `lastsync` (I32, UTC seconds of the last successful sync; internal, persisted so TIME UNSYNC survives reboots).
- **Migration 2→3**: `tzid` = the `Zones::bestMatch(tzoff)` entry (a named zone with no DST and that offset if one exists,
  e.g. 180 → Istanbul, otherwise the fixed entry), then `tzoff` is removed.
- Secrets are honoured by the existing `formatSettingForOutput` (`***`), `ad set list`, and the Settings page (`••••`).
  `SettingsStore` never logs values (S0).
- **Known limitation**: NVS is not encrypted (flash encryption is off for this hobby device). Credentials are readable
  by anyone with physical flash access. This is documented and acceptable for S2 (constitution VI requires NVS-only
  and never logged, both met).

---

## R5. Phone setup hotspot (FR-004, FR-004a–c, SC-008)

**Decision**:
- `WiFi.mode(WIFI_AP_STA)`. Scan first (`WiFi.scanNetworks`, results cached, strongest first, duplicates merged), then a SoftAP
  `AeroDeck-XXXX` (last 4 hex of the MAC) with a **random 8-character WPA2 access code** (`esp_random`, from an
  unambiguous alphabet). The code is shown only on the watch.
- `DNSServer` answers every name with `192.168.4.1`, so phones open the page automatically ("sign in to network").
  `WebServer` on port 80 serves:
  - `GET /`: the setup page (single embedded HTML ≤6 KB, no external resources, works on iOS/Android);
  - `GET /scan.json`: the network list;
  - `POST /save` (ssid, psk): validated lengths;
  - captive-portal probes (`/generate_204`, `/hotspot-detect.html`, …): redirect to `/`.
- **Watch UI**: a modal on the Settings page shows the SSID and access code, an **LVGL QR code** (`LV_USE_QRCODE=1` in
  LilyGoLib's `lv_conf.h`) encoding `WIFI:T:WPA;S:<ssid>;P:<code>;;` (phone cameras join directly), the URL
  `http://192.168.4.1`, a 5-min countdown and Cancel.
- On save: stop the AP and DNS, persist the credentials, run the test connection (R1), and show the result on the watch.
  Auto-close after 300 s. It always ends with Wi-Fi OFF.
- The `WebServer.handleClient()` loop runs on the `net` task, not the UI.

**Rationale**: No app needed. The QR code removes typing the access code. The synchronous WebServer on its own task
is simplest.

**Alternatives**: ESPAsyncWebServer (new dependency, needs pinning), BLE provisioning (needs a phone app), an on-watch
keyboard (rejected by the product owner).

---

## R6. Serial command quoting (FR-004 USB path)

**Finding**: `core::CommandParser` splits on spaces, but SSIDs may contain spaces.

**Decision**: The tokenizer accepts **double-quoted tokens** (`"My Home WiFi"`, with `\"` and `\\` escapes). Host
tests are extended. Commands:
- `ad wifi set "<ssid>" "<psk>"`, `ad wifi`, `ad wifi forget`, `ad wifi setup`, `ad wifi test`, `ad wifi scan`;
- `ad sync`;
- `ad tz`, `ad tz list [filter]`, `ad tz set <name|index>`.

The 128-char line limit fits `ad wifi set` + 32 + 63 + quotes (≤108).

---

## R7. Sync scheduling and status (FR-005, FR-017, FR-018)

**Decision**:
- `TimeSync` logic (pure `core::SyncSchedule`, host-tested) decides when the next sync is due:
  - at boot, if credentials exist;
  - then every `syncmin`;
  - after a failure, retry with backoff (2, 5, 15 min, then the normal interval).
- `NetStatus` snapshot fields: `configured`, `lastResult` (Ok, WrongPassword, NotFound, NoTimeResponse, Timeout, Rejected),
  `lastSyncUtc`, `lastAttemptMs`, `setupActive`, `setupRemainingS`.
- TIME UNSYNC: a `Cautions` ADVISORY-style entry (the S0 banner is shared until S4's AlertManager). Raised when
  `configured && (lastsync == 0 || now − lastsync > 24 h)`. Cleared on success.
- Failure reasons come from `WiFi.status()` / the disconnect reason (`WIFI_REASON_AUTH_FAIL`/`_NO_AP_FOUND` via the event
  handler) and from NTP timeouts.

---

## R8. Time-zone selection UI (FR-013, FR-014)

**Decision**: The Settings page "Time zone" row shows the current zone name. Tapping opens an `lv_roller` (45 named
zones first, then the fixed offsets). It applies on release, and the PFD's LOCAL updates on the next minute key (≤1 s,
because `ClockSnapshot` is refreshed on `SettingsChanged`). The "Local offset" row from S1 is removed.

---

## R9. Constitution and SRS impact

- Constitution III: Wi-Fi is leased (R1). VI: secrets in NVS, never logged, masked (R4); the hotspot is deliberate, time-boxed
  and code-protected (R5); HTTPS is not applicable (SNTP/UDP and a local AP page; no remote HTTP API in S2). VII: no new
  libraries (framework built-ins only).
- SRS: FR-TIM-003 interim note → replaced by named zones; FR-TIM-004 implemented; a new **FR-NET-003** covers the setup
  hotspot (deliberate start, one-time code, 5-min auto-close, no overlap with sync). Added in this branch.
