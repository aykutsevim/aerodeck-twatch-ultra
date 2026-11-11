# Research: METAR Weather Page (Slice S3)

All findings were checked on 2026-11-11 against the live provider (from the development PC) and the installed toolchain
(`framework-arduinoespressif32` 2.0.17, ESP32-S3 sdkconfig).

## R1 · Provider request and response format
- **Decision:** `GET https://aviationweather.gov/api/data/metar?ids=<ICAO>&format=raw` over HTTP/1.1 with
  `Connection: close` and a short `User-Agent` (`AeroDeck/<version>`).
  - The response is `text/plain` with a `Content-Length` and one line per report, newest first, e.g.
    `METAR LTAC 111250Z 27007KT 9999 FEW016 BKN030 10/04 Q1020 NOSIG`.
  - **HTTP 204 No Content** means an unknown station or no recent report (verified with `ids=ZZZZ`).
  - The response may contain RMK/trend groups. `hours=3` returns several lines.
- **Rationale:**
  - The raw text is the one thing the page must show anyway (FR-008), and the decoder needs it (FR-005).
  - The JSON form adds 400+ bytes and pre-decoded fields we'd still have to cross-check, plus a JSON library
    (constitution VIII: no dependency without need).
  - The observation time comes from the `DDHHMMZ` group (R5).
- **Alternatives:**
  - `format=json` + ArduinoJson (pinned): rejected. It means a new dependency and larger responses, and its `fltCat`
    is useful only as a cross-check, which the off-device tests already do (S3 tests compare with the provider's
    category for the sample set).
  - `format=xml`: larger, no benefit.

## R2 · HTTPS client and certificate verification (NFR-SEC-002, FR-002)
- **Decision:** framework `WiFiClientSecure` with `setCACert(<PEM bundle>)` and a hand-written minimal HTTP/1.1
  exchange. The request line goes out, then response bytes are read into a bounded 2 KB buffer and parsed by the pure
  `core::HttpResp` (status line, `Content-Length`, `Transfer-Encoding: chunked` → treated as unsupported/error, header
  end, body).
  - The leaf is issued by **GeoTrust TLS RSA CA G1**, which chains to **DigiCert Global Root G2**
    (verified with `openssl s_client`). The server negotiates TLS 1.3 with the PC. The ESP32 (mbedTLS 2.28) uses
    TLS 1.2, which the server also accepts (verified on the device in the first implementation checkpoint).
  - The PEM bundle `src/services/WxCerts.h` contains **DigiCert Global Root G2** plus **DigiCert TLS RSA4096 Root G5**
    as a fallback for a CDN rotation. *(Implementation: the planned "DigiCert Global Root CA" is no longer in
    certifi; it was retired from public trust stores, so DigiCert's current RSA root is used instead.)* The header
    records each subject, SHA-256 fingerprint and expiry; the source is the PlatformIO Python `certifi` store. Both
    roots alone verify the live server from the PC (Python `ssl`, TLS 1.3 and TLS 1.2).
- **Rationale:**
  - It's built into the framework, so there's no new library.
  - A bounded buffer and a pure parser make malformed or oversized responses safe and host-testable (FR-020).
  - `HTTPClient::getString()` has no size bound.
- **Alternatives:**
  - `setInsecure()`: rejected (FR-002).
  - The IDF certificate bundle (`setCACertBundle` + an embedded `x509_crt_bundle`): this means extra build plumbing
    (`board_build.embed_files`) and ~60 KB of flash for 130 roots. It's not needed for one provider. Revisit if more
    HTTPS providers arrive (S9+).
  - `HTTPClient`: no body bound, and harder to test.

## R3 · Heap during TLS (constitution IV: ≥40 KB internal)
- **Finding:** the framework sdkconfig sets `CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC=1`, so all mbedTLS allocations come
  from **internal** RAM:
  - two record buffers (16 KB in + 4 KB out);
  - the handshake, peak ≈ 35–45 KB in total.

  S2 measured heapmin 81.9 KB with Wi-Fi connected. TLS would bring it to ≈ 37–47 KB, which is at or under the
  40 KB gate.
- **Decision:** once, in `WeatherService::begin()` (before Wi-Fi is first used), call
  `mbedtls_platform_set_calloc_free(wxCalloc, wxFree)` with a **size-based** allocator:
  - blocks of **>= 4096 B** come from `heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`, falling back
    to internal RAM if PSRAM fails. These are the TLS record buffers (16 KB in, 4 KB out) and the large handshake
    buffers;
  - smaller blocks stay in **internal** RAM (`MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`), the same as the default;
  - `wxFree` = `heap_caps_free`, which works for either region.

  Notes:
  - This is a supported mbedTLS hook (`MBEDTLS_PLATFORM_MEMORY` is enabled in IDF 4.4).
  - **mbedTLS is not used only by the `net` task:** the IDF 4.4 Wi-Fi supplicant (`CONFIG_WPA_MBEDTLS_CRYPTO`) also
    calls it from the Wi-Fi task (WPA handshake, rekey), possibly during a TLS session. The hook is process-wide.
    It's installed once and never swapped, and its small allocations keep the default internal behaviour, so the
    supplicant sees no change in practice. Verify with S2's boot-sync and setup checks (T035).
  - Measure in Q5: heapmin during 10 fetches must be ≥40 KB. Handshake time is expected at ~1–2 s with PSRAM (also
    measured).
- **Fallback (if PSRAM TLS is too slow or unstable):** keep internal allocation, but use
  `mbedtls_ssl_conf_max_frag_len` (512 B), if the server accepts it, to shrink the record buffers. Otherwise lower the
  Wi-Fi RX buffers during a fetch. The choice is recorded in validation.md.
- **Alternatives:**
  - Rebuilding the framework with a custom sdkconfig (Arduino as an IDF component): rejected as a large build change
    (constitution VII).

## R4 · Integrating with the S2 net task (FR-001, FR-003, FR-004)
- **Decision:**
  - `NetService` gains `Cmd::Weather`, and `Cmd::Sync` gains a weather step.
  - **Sync:** lease Wi-Fi → connect (shared 15 s connect+NTP budget, unchanged) → SNTP → set the clock →
    **`WeatherService::fetchOnNet(deadline = now + 15 s)`** → release the lease (Wi-Fi OFF). The weather step runs
    whenever the connection succeeded, even if NTP failed (FR-004: independent results). Worst-case session time is
    15 s + 15 s = 30 s.
  - **Weather (on demand):** lease → connect → fetch → off, with **one 15 s deadline for the whole transaction**
    (connect + fetch share it; spec FR-003, SC-004). A slow connect leaves less time for the fetch. A refresh result
    therefore always arrives ≤15 s after the request starts, within SC-003's 20 s.
  - `NetBusy` gains `Weather`. The S2 rule "the setup hotspot never overlaps a transaction" holds automatically
    (single queue, single task).
  - Coalescing: `requestWeather()` sets an atomic `s_wxPending` flag and posts `Cmd::Weather` only if it was not
    already set. A Sync whose weather step runs also clears the flag. When `Cmd::Weather` is **dequeued**, it
    atomically tests and clears the flag, and **skips** if the flag was already cleared (a Sync already fetched). So a
    burst of taps, or a tap followed by a sync, produces exactly one fetch (FR-015).
- **Rationale:** one radio session per hour (constitution III duty cycle ≈ ≤30 s/60 min, normally ~12 s ≈ 0.3 %).
  No new task, and the S2 queue gives mutual exclusion.
- **Alternatives:**
  - A separate weather task: rejected (two radio users, more RAM).
  - A separate hourly weather schedule: rejected (an extra radio session per hour).

## R5 · Observation time from `DDHHMMZ`
- **Decision:** pure `core::Metar::resolveObsTime(dd, hh, mm, nowUtc)` returns the latest UTC instant with that
  day-of-month/hour/minute that is ≤ `nowUtc + 10 min`, searching the current and previous month. It handles month
  ends (e.g. day 31 seen on the 1st) and year rollover.
  - If the watch clock is not set (`nowUtc` invalid), the report keeps `obsUtc = 0` → STALE (spec US2-5).
  - The 10 min allowance tolerates a slightly slow watch clock.
- **Alternatives:**
  - The HTTP `Date` header plus DDHHMM: same logic, extra parsing; not needed while the S2 clock is synced first.
  - JSON `obsTime`: see R1.

## R6 · METAR decoding rules (FR-005, FR-007)
- **Decision:** a pure, single-pass tokenizer `core::Metar::decode(const char* raw, Report&)`. It stops decoding at
  `RMK`, `NOSIG`, `BECMG`, `TEMPO` and `TAF` (those stay in the raw text). Groups:

  | Item | Group | Rule |
  |---|---|---|
  | Header | `METAR`/`SPECI` (optional), station `[A-Z]{4}`, `DDHHMMZ`, `NIL` | `NIL` → `Report.nil = true` |
  | Modifiers | `AUTO`, `COR` | ignored |
  | Wind | `dddff(f)(Gff(f))KT\|MPS` | `ddd` = 000–360 or `VRB`; `00000KT` → calm; MPS × 1.944 → kt (rounded); `/////KT` → missing |
  | Wind sector | `dddVddd` | variable sector |
  | Visibility | `CAVOK`; `dddd` (metres, `9999` → ≥10 km); `[M\|P]n[ n/d]SM` incl. `1 1/2SM` (two tokens), `M1/4SM`, `P6SM`; `////` → missing | directional `dddd[NSEW]{1,2}` minimum ignored; the prevailing (first) value is used |
  | RVR | `R..` | skipped |
  | Weather | `[+-]?(VC)?[A-Z]{2,8}` phenomena | skipped |
  | Clouds | `(FEW\|SCT\|BKN\|OVC)hhh(CB\|TCU\|///)?`, `VVhhh`, `NSC`, `NCD`, `SKC`, `CLR`, `///hhh` | heights × 100 ft; up to 6 layers kept |
  | Temperature | `(M)?tt/(M)?dd` | `M` = negative; `//` → missing |
  | Pressure | `Qpppp` (hPa); `Apppp` (inHg × 100 → × 33.8639 / 100 hPa, rounded) | missing → `--` |

  - **Ceiling:** the lowest BKN/OVC/VV layer. FEW/SCT, NSC, SKC, CLR and CAVOK mean no ceiling (unlimited).
  - **Flight category:** the worse of the ceiling category and the visibility category. Thresholds are as in spec
    FR-007; metric visibility compares in metres against 1600/4800/8000 m.
  - **Unknown rule (spec FR-007):**
    - visibility missing → `Category::Unknown`;
    - cloud information missing (no cloud group and no CAVOK/NSC/NCD/SKC/CLR) → `Unknown`, **unless the visibility
      alone gives LIFR**, which is then the result. A ceiling can only make the category worse, and LIFR is already
      the worst. IFR from visibility alone is still `Unknown`, because a low ceiling could make it LIFR;
    - cloud layers present but none BKN/OVC/VV → no ceiling (unlimited).
  - Every numeric field is range-checked (wind 0–360°/0–250 kt, visibility 0–99 999 m, temp −80…+60, QNH 850–1090);
    a value out of range counts as missing (FR-020).
- **Test corpus (FR-018, SC-001):** ≥20 real reports collected from the provider at implementation time, chosen to
  cover every row above. Examples: LTAC (current), plus stations with US units (KSFO/KJFK), CAVOK (LTAI/LEMD),
  negative temperatures (UUEE/PANC/CYYZ), VV (fog), gusts, VRB, variable sector, AUTO, `NIL`, and MPS (ZBAA/UUEE).
  Each one is stored in the test file with the expected decode and the provider's JSON `fltCat` for the category
  cross-check.

## R7 · Cache across reboots (FR-013)
- **Decision:** `SettingsStore` gains an internal **blob** API `getBlob(key, buf, cap)` / `setBlob(key, data, len)` for
  non-setting cached data in the `aerodeck` NVS namespace. It's not part of the settings schema and not listed by
  `ad set list`.
  - WeatherService stores `wxcache` = `{u8 version, char station[5], i64 fetchedUtc, i64 obsUtc, u16 rawLen, char raw[≤256]}`.
  - At boot it reloads and re-decodes the raw text, which keeps the decoder authoritative.
  - The cache is cleared when the station changes. Writes happen only after a successful fetch with a new
    observation time (≤ 1 per hour, flash-friendly).
- **Alternatives:**
  - A file on FATFS: the partition is used for assets; NVS is simpler for <300 bytes.
  - The S2 `Str` setting type (64-character cap): too short for a raw METAR.

## R8 · Station setting
- **Decision:** a new setting `wxstn`, type `Str`, length 4..4, validated `[A-Z]{4}` (input uppercased), default
  `"LTAC"`, editable.
  - `SettingDef` gains `const char* sdef` (the string default). This is additive.
  - Schema **v4**: an additive migration (the existing "key added by the new schema → persist default" path).
  - No data conversion.
- On `SettingsChanged(WxStation)`, WeatherService clears the snapshot and cache and calls
  `NetService::requestWeather()` (if configured).
- Serial: `ad wx set <ICAO>` → `SettingsStore::setStr`. Settings page: a "Weather station" row → a modal with four
  `lv_roller`s (A–Z), OK/Cancel.

## R9 · WX page layout (410×502, r = 64)
- **Decision:** `WxPage` replaces `PlaceholderA` in the page ring (PFD → WX → PAGE B → SETTINGS), and is built with
  `S(px)` and the S1 rounded-corner helpers (`minTopForRounded`).
  - **Top band:** the station, the observation time `1250Z`, and a category pill (VFR/MVFR/IFR/LIFR/-- in category
    colours).
  - **Status row:** the STALE / error annunciator (amber box) and the fetched time.
  - **Middle:** a compass rose on the left (`S(150)` diameter, `lv_arc` ring + N/E/S/W labels + an arrow drawn as an
    `lv_line` rotated by recomputing its points; calm/VRB text), and on the right, a tile grid: WIND, VIS, QNH,
    T/DP, CLOUDS.
  - **Bottom:** the raw text in a scrollable, wrapping label inside the safe area, and a **REFRESH** button.
  - Category colours become new Theme tokens: `catVfr()` green `#00C000`, `catMvfr()` blue `#3A7BFF`, `catIfr()` red
    `#FF3030`, `catLifr()` magenta `#FF40FF`, `catUnknown()` = `dim()`. NVG variants follow in a later slice.
  - The page updates only when the `WeatherSnapshot.seq` changes, or once per 30 s for STALE (FR-011: ≤60 s). No
    per-frame work, so no FPS impact.
- **Alternatives:** a canvas-drawn compass: more PSRAM and redraw cost, no benefit.

## R10 · Fetch result classification
- **Decision:** `WxResult { None, Ok, NoWifi, NetFail, NoReport, ServerError }` (the busy state lives in `WxBusy`, data-model §3), mapped from:
  - no credentials → `NoWifi`;
  - connect failure → `NetFail`;
  - DNS/TCP/TLS failure (including certificate rejection) → `NetFail` (logged distinctly: `tls`);
  - HTTP 204, an empty body, a `NIL` report, or a body whose station ≠ requested → `NoReport`;
  - HTTP ≠ 200/204, body > 2 KB, chunked encoding, or the decoder finding no station/time → `ServerError`;
  - deadline exceeded → `NetFail`.

  Display text follows spec FR-012.
