# Implementation Plan: METAR Weather Page (Slice S3)

**Branch**: `004-metar-weather` | **Date**: 2026-11-11 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/004-metar-weather/spec.md`

**Implements**:
- AD-SAS-001 §5.1 (`net` task: Wi-Fi up → NTP → **METAR** → Wi-Fi down);
- §5.2 (`WeatherSnapshot`, `WeatherUpdated`);
- §5.3 (hourly sync sequence with `WeatherService::fetch()`);
- §6 (`WeatherService`, `WxPage`);
- §7.1 (settings: METAR station);
- ADR-007 (aviationweather.gov, no key).

Builds on S2's NetService queue and lease, and on the S1 theme, layout and rounded-corner helpers.

## Summary

The watch fetches the METAR for a configured ICAO station (default LTAC):
- **when:** inside the S2 sync radio session (boot and hourly), and on demand (refresh button, station change,
  `ad wx fetch`);
- **how:** HTTPS from aviationweather.gov (`format=raw`). The server certificate is verified against built-in DigiCert
  roots. TLS memory goes to PSRAM so internal heap stays ≥40 KB.

A pure decoder (`core::Metar`) extracts wind, visibility, clouds/ceiling, temperature/dewpoint and QNH, and computes
the flight category. `core::HttpResp` safely parses the bounded HTTP response. Both are host-tested against ≥20 real
reports.

`WeatherService` publishes a `WeatherSnapshot` and keeps the last good report in NVS. A new **WX page** shows:
- the decoded tiles and the category pill;
- a wind compass;
- the raw text;
- STALE and error annunciators;
- a REFRESH button.

The station is set from Settings (a four-letter roller modal) or `ad wx set`.

## Technical Context

**Language/Version**: C++17 (xtensa GCC 8.4, Arduino-ESP32 2.0.17 / IDF 4.4). Host tests use WinLibs GCC 16.2.

**Primary Dependencies**:
- Framework built-ins only: `WiFiClientSecure` (mbedTLS 2.28) and `Preferences`;
- LVGL 9.6 (`lv_roller`, `lv_arc`, `lv_line`, label wrap/scroll);
- **No new libraries** (R1: no JSON parser needed).

**Storage**:
- NVS settings schema **v4** (`wxstn`, additive);
- an NVS blob `wxcache` (≤ 285 B), written at most once per new observation.

**Testing**:
- Unity on `env:native`:
  - `test_metar`: ≥20 real reports, resolveObsTime, isStale, format helpers, category boundaries, fuzz-ish
    malformed input;
  - `test_http_resp`;
  - `test_settings` (v4, `wxstn` validation).
- On-device: quickstart Q2–Q9.

**Target Platform**: T-Watch Ultra (410×502, r = 64). The T-Watch S3 must compile (no board dependency).

**Project Type**: Embedded firmware.

**Performance Goals**:
- On-demand fetch ≤15 s **including** the Wi-Fi connect; weather step inside a sync ≤15 s (session ≤30 s); typically
  1–3 s for the HTTPS part;
- boot to fresh report ≤30 s;
- refresh feedback ≤100 ms;
- PFD FPS median ≥28 during fetches (S2 baseline);
- WX page update without per-frame cost.

**Constraints**:
- Internal heap ≥40 KB during TLS (R3: mbedTLS allocator → PSRAM; measured in Q5);
- Wi-Fi OFF outside transactions;
- one radio session per sync;
- a response body ≤2 KB;
- no blocking on the UI task;
- HTTPS with certificate verification.

**Scale/Scope**:
- 2 core modules (`Metar`, `HttpResp`);
- 1 new service (`WeatherService`) + NetService changes;
- 1 new page (`WxPage`) + 1 Settings modal;
- ~7 serial commands;
- 1 settings key + 1 blob.

No NEEDS CLARIFICATION remains (research R1–R10). The heap figure during TLS is **measured by design** (R3, Q5).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.* (Constitution v1.0.2)

| # | Principle | Pre | Post | Evidence |
| :-- | :--- | :---: | :---: | :--- |
| I | Every slice ships | ✅ | ✅ | No network/station/report → the page shows NO DATA or the cached report with a reason. Every failure is bounded (15 s) and non-fatal. The PFD is unaffected; the ring keeps PFD first |
| II | Layered architecture | ✅ | ✅ | Decoding and HTTP parsing are pure `core/`. TLS/socket code only in `services/WeatherService` (ADR-012 platform service, run on the `net` task). UI reads `Snapshot<WeatherSnapshot>` and calls `NetService::requestWeather()`. No LVGL off the UI task. No board macros |
| III | Battery is a feature | ✅ | ✅ | No new radio session for scheduled updates: weather rides the S2 sync lease. **Duty cycle:** typically ≈12 s per 60 min (connect ~8 s + NTP + TLS fetch ~2 s) ≈ 0.3 %; worst case 30 s per 60 min (0.8 %). On-demand fetches are coalesced (one per tap burst). Wi-Fi OFF afterwards (logged) |
| IV | Frame time is sacred | ✅ | ✅ | All network/TLS work on the core-0 `net` task. The WX page updates on snapshot change or every 30 s only. TLS buffers move to PSRAM (R3) to keep internal heap ≥40 KB. FPS and heap measured in Q5 |
| V | Test off-device first | ✅ | ✅ | `core::Metar` and `core::HttpResp` have Unity tests written first: ≥20 real reports, cross-checked against the provider's category. Settings v4 tested. On-device checks Q2–Q9 come from the acceptance scenarios |
| VI | Deliberate, compliant, private | ✅ | ✅ | HTTPS with certificate verification (NFR-SEC-002). No credentials involved (no API key in S3). The request carries only the station code and a generic User-Agent. Nothing transmitted besides the Wi-Fi traffic S2 already allows |
| VII | Reproducible, portable | ✅ | ✅ | No new dependencies. The root certificates are committed with source and expiry noted. The S3 board compiles (the code is board-agnostic) |
| VIII | Simplicity | ✅ | ✅ | Raw text format instead of JSON + library. A hand-rolled bounded HTTP/1.1 GET instead of HTTPClient (the bound is a requirement, FR-020). Single provider; the CheckWX provider interface is deferred (YAGNI) |

**Gate: PASS.** Complexity Tracking is not needed.

**SRS amendments in this branch** (workflow gate 2):
- new **FR-WX-006** (M): "The METAR station shall be selectable on the watch and over the diagnostic console,
  validated as a 4-letter ICAO code, and persisted (default LTAC); a change triggers a fetch";
- FR-WX-001 note: the weather is fetched inside the hourly sync radio session;
- S3 traceability row updated.

**SAS notes**:
- §6 WeatherService row (PSRAM TLS allocator, raw format, NVS cache);
- §7.1 settings v4 (`wxstn`, `wxcache` blob).

No ADR is needed: the design follows ADR-007 and ADR-012.

## Project Structure

### Documentation (this feature)

```text
specs/004-metar-weather/
├── plan.md  research.md  data-model.md  quickstart.md
├── contracts/{serial_commands.md, wx_page.md, provider_http.md}
├── checklists/requirements.md
└── tasks.md            # /speckit-tasks
```

### Source Code (new or changed)

```text
src/core/
├── Metar.{h,cpp}           # decode, ceiling, category, resolveObsTime, isStale, format helpers, pickNewest (pure)
├── HttpResp.{h,cpp}        # bounded HTTP/1.1 response parser (pure)
├── SettingDefs.{h,cpp}     # + WxStation (Str, sdef "LTAC"), validateString, schema 4
└── Types.h                 # + PageId::Weather (replaces PlaceholderA)
src/services/
├── WeatherService.{h,cpp}  # snapshot, cache, fetchOnNet(deadline) [net task], PSRAM mbedTLS allocator, `ad wx …`
├── WxCerts.h               # DigiCert Global Root G2 + Global Root CA (PEM)
├── NetService.{h,cpp}      # + Cmd::Weather, NetBusy::Weather, requestWeather() (coalesced), weather step in Sync
├── SettingsStore.{h,cpp}   # + string defaults, getBlob/setBlob/eraseBlob, restore-defaults clears wxcache
└── EventBus.h              # + WeatherUpdated
src/ui/Theme.{h,cpp}        # + catVfr/catMvfr/catIfr/catLifr/catUnknown tokens
src/pages/
├── WxPage.{h,cpp}          # new page (contracts/wx_page.md)
└── SettingsPage.{h,cpp}    # + "Weather station" row + 4-roller modal
src/app/main.cpp            # WeatherService::begin; WxPage replaces g_pageA
test/
├── test_metar/  test_http_resp/
└── test_settings/          (extended: v4)
docs/software_requirements_specification.md   # FR-WX-006, FR-WX-001 note, traceability
docs/software_architecture_specification.md   # WeatherService/SettingsStore rows
README.md                                     # WX page, station, `ad wx` commands
```

**Structure Decision**: Continue the S0–S2 layout. The weather fetch is a function executed by the `net` task, the same
pattern as `SetupPortal`.

## Implementation Notes (for /speckit-tasks)

1. **Order:**
   1. Test corpus + `core::Metar`/`core::HttpResp` tests → implementation → settings v4 + blob API.
   2. WeatherService fetch (TLS, PSRAM allocator) + `ad wx` commands + the NetService integration.
   3. **Checkpoint on the device:** `ad wx fetch` works, the cert is verified, heapmin ≥40 KB.
   4. WxPage.
   5. Settings station modal.
   6. Cache/STALE/errors.
   7. Docs.
   8. Validation.
2. **Test corpus:** fetch ≥20 live reports covering R6's rows. Use `format=json` once (PC script) to record the
   provider's `fltCat` next to each raw string as the category oracle. Store them inline in
   `test/test_metar/test_main.cpp` with the expected fields. Record the collection date.
3. **Size-based TLS allocator (R3):** `mbedtls_platform_set_calloc_free()` is called once in `WeatherService::begin()`,
   before Wi-Fi is first used, and never swapped.
   - Blocks ≥4 KB go to PSRAM; smaller blocks stay internal.
   - The hook is process-wide, and the Wi-Fi supplicant also uses mbedTLS (WPA handshake/rekey on the Wi-Fi task), so
     small allocations must keep the default internal behaviour.
   - Verify that S2's boot sync and setup hotspot still work (T035).
   - If the handshake fails, or exceeds 5 s with PSRAM, apply the R3 fallback and document it.
4. **Certificate check:** verify the chain on the device with a deliberately wrong root (a temporary build flag
   `-DWX_CERT_SELFTEST`, or `ad wx` printing the verification result) → `NetFail tls`. Then restore. Note in
   validation.md.
5. **Station change during a fetch:** WeatherService stores the station it is fetching for. On completion it discards
   the result if `wxstn` changed, and re-requests.
6. **Clock dependency:** `resolveObsTime` uses `TimeService` UTC. In the sync sequence the clock is set before the
   weather step, so a cold boot with a reset RTC still resolves correctly. If NTP failed and the clock is invalid,
   `obsUtc = 0` → STALE (spec US2-5).
7. **Page ring:** replacing `PlaceholderA` with `WxPage` changes S0/S1 nav expectations (`PAGE A` → `WX`). Update the
   S1 regression script expectations and `ad nav` names.
8. **Measure (Q5):** heapmin during 10 fetches, fetch duration, session length, and FPS median with sweep on. If heap
   <40 KB after R3, use the fallback. Record everything in validation.md.
9. **At the end:** update `docs/product_roadmap.md` per the status tracking rule (S3 → ✅ Done after merge; M1 → ✅ when
   S3 merges and the DoD is met).

## Complexity Tracking

Not needed. The Constitution Check passed with no violations.
