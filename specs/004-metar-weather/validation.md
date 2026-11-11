# S3 Validation: METAR Weather Page

**Device:** T-Watch Ultra (COM3), firmware from branch `004-metar-weather` · **Date:** 2026-11-11
**Network:** the user's Wi-Fi configured in S2 (credentials unchanged during these tests; SSID not recorded).
Scripts (session scratchpad): `wx_corpus.py`, `wx_pick.py`, `gen_wxcerts.py`, `verify_roots.py`, `wxcheck.py`,
`wx_boots.py`, `wx_perf.py`, `validate_s1.py`.

## Summary

| Criterion | Result | Evidence |
|---|---|---|
| SC-001 20/20 decodes correct | ✅ 24/24 real + live | §Corpus: every field and the category match the provider's decode; live LTAC/LTFM reports decoded on the device (`ad wx fields`) |
| SC-002 fresh report ≤30 s after boot, 10/10 | ✅ 10/10 | §Q3: 12.1–19.7 s after reset |
| SC-003 refresh feedback ≤100 ms, result ≤20 s, smooth | ✅ | §Q5: request → Wi-Fi off 5.6–9.8 s; FPS median 28.0 during fetches (baseline 29). The ≤100 ms "REFRESHING..." feedback is drawn in the same LVGL cycle as the tap (code); **visual check by the user pending** |
| SC-004 network unreachable: 10/10 give up ≤15 s | ⏳ pending (user) | Needs the router's internet cut (T023 step 3). Bounded by design: one 15 s deadline for connect + fetch |
| SC-005 cached report visible ≤5 s after the face | ✅ (network present) | The cache loads at 3.61 s, before `boot->home` (3.72 s), on every boot. The no-network variant is part of T023 step 3 |
| SC-006 24 h: 24/24 fetches | ⏳ pending | T036 (24-h soak, can run with S2 T038) |
| SC-007 station change from Settings <30 s, report ≤20 s | ✅ serial · ⏳ touch | §Q7: the new station's report arrived 4–6 s after the change; the Settings picker needs a touch test by the user |

## Corpus (T003, FR-018)
- Collected 2026-11-11 from `https://aviationweather.gov/api/data/metar?format=json&hours=24` for 30 stations
  (LTAC, LTFM, LTAI, LEMD, LGAV, KSFO, KJFK, KORD, KDEN, KBOS, UUEE, UNNT, ZBAA, PANC, CYYZ, ENGM, EGLL, EDDF, LFPG,
  KSEA, PAFA, CYZF, BIKF, RJTT, YSSY, KLAX, KMIA, EHAM, UHHH, ZSPD): 400 reports.
- 24 were chosen to cover every decoder group: gusts, VRB, variable sector, calm, MPS, CAVOK, SM fractions, `M1/4SM`,
  VV, negative temperatures, inHg, AUTO, NSC/NCD/SKC, CB/TCU, directional visibility, `///` cloud suffixes, COR, SPECI,
  TEMPO, and all four categories.
- **All 24 decode to the provider's own values** (category, wind, visibility, QNH, temperature/dewpoint). Synthetic
  reports cover NIL, P6SM, `////`, `/////KT`, `///015`, out-of-range values and malformed input.
- Provider conventions the decoder follows:
  - US visibility is compared in statute miles, so 5 SM is MVFR (8047 m would otherwise exceed 8000 m);
  - the directional minimum is ignored (`9000 3000SW` is VFR at UUEE).

## Phase 2 / regression
- `pio test -e native`: **142/142** (after the readable-text helpers):
  - test_metar: 12 tests, 24-report corpus;
  - test_http_resp: 11;
  - test_settings: v4;
  - test_command_parser: +long quoted METAR;
  - all S0–S2 suites.
- `pio run -e twatch_ultra` and `pio run -e twatchs3`: SUCCESS. Ultra: RAM 18.4 %, Flash 47.3 %.
- Layering grep: Wi-Fi/TLS/mbedTLS headers appear only in `src/services/`; no Arduino/IDF/LVGL headers in `src/core/`;
  no `lv_` in services. Secrets grep: no PSK in any log or reply.
- S1 regression (`validate_s1.py att perf regress`):
  - PFD median 29 fps (min 28) with sweep;
  - attitude OK, and the IMU simfail raises IMU FAIL;
  - page ring **PFD → WX → PAGE B → SETTINGS**, swipes 238–350 ms;
  - all peripherals OK, boot→home 3718 ms.
- S2 boot sync: `[net] sync: Ok` on every reboot during these tests (more than 15 boots).

## Implementation notes (deviations from the task text)
- `PageId` lives in `src/ui/Page.h`, not `core/Types.h` (T011). `PlaceholderA` was renamed `Weather`; no test referred
  to it.
- Fallback root: **DigiCert TLS RSA4096 Root G5** instead of "DigiCert Global Root CA", which is no longer in certifi
  (research R2 updated). Both roots alone verify the live server from the PC (TLS 1.3 and 1.2).
- `core::HttpResp::parse` returns a three-state `Result` (data-model §2 updated).
- Serial line limit raised 128 → 300 characters and the log line buffer 160 → 288, so a full raw METAR fits
  (`ad wx decode "<METAR>"`, the `[wx] raw …` log line).

## T017 checkpoint: HTTPS, heap, certificate
- `ad wx fetch` → `[wx] fetch: Ok stn=LTAC obs=1320Z cat=MVFR`. HTTPS time 0.7–2.7 s (typically ~1.2 s, PSRAM TLS
  buffers).
- Internal heap minimum **59.0 KB** over 15 fetches with the PFD sweep, and 59.2 KB at boot with Wi-Fi + TLS
  (gate ≥40 KB). The size-based allocator (research R3) is enough; no fallback is needed.
- **Certificate self-test** (temporary build `-DWX_CERT_SELFTEST`, trusting only the G5 root): every fetch failed with
  `connect/tls failed err=-9984 (X509 - Certificate verification failed)` → `NetFail`, and the last good report stayed.
  Normal firmware was then re-flashed.
- **One radio session per sync:** boot log `rail wifi ON` 3.62 s → `sync: Ok` 8.18 s → `wx fetch: Ok` 9.21 s →
  `rail wifi OFF` 10.85 s (7.2 s session).

## Q3: 10 boots (SC-002)

| Boot | Fresh report after reset (s) | HTTPS (ms) | Wi-Fi after |
|---|---|---|---|
| 1 | 19.7 | 9128 | OFF |
| 2 | 13.8 | 2060 | OFF |
| 3 | 12.2 | 1353 | OFF |
| 4 | 15.5 | 4415 | OFF |
| 5 | 12.3 | 1553 | OFF |
| 6 | 12.3 | 1225 | OFF |
| 7 | 12.1 | 1242 | OFF |
| 8 | 13.8 | 1927 | OFF |
| 9 | 12.2 | 1081 | OFF |
| 10 | 12.2 | 1038 | OFF |

**10/10 pass** (≤30 s). Times are measured from reset to `ad wx` showing `last=Ok stale=0`, with ~1 s polling
granularity.

## Q5: refresh, coalescing, performance (US3)
- PFD sweep baseline: median **29**, min 27 (n=19).
- During 10 on-demand fetches: median **28.0**, avg 26.8, min 20 (n=175). The dips are at Wi-Fi association, as in S2;
  the median meets the S2-accepted ≥28.
- Request → Wi-Fi off: 7.3–9.8 s for 8 of 10 fetches. Two runs waited the full 25 s because the serial link lost the
  result line (USB CDC byte drops seen before); those fetches themselves completed.
- Coalescing: two `ad wx fetch` back to back → `OK fetching`, `OK already pending`, and exactly one `[wx] fetch` line.

## Q6: failures, STALE, cache (US2)
- Unknown station `ZZZZ` → `[wx] fetch: NoReport`; page state `has=0` (NO DATA), `last=NoReport`.
- **Station change during a fetch** (set to ZZZZ while the boot sync's LTAC fetch was running) → `fetch for LTAC
  discarded (station now ZZZZ)`, then the ZZZZ fetch ran (edge case, data-model §3).
- `ad wx age 6000` → `stale=1`; `ad wx age 5300` → `stale=0`; `-1` → off.
- Clock not set (`ad time set 2020-01-01…`, implausible year) → `stale=1`. `ad sync` → clock restored, the weather was
  fetched in the same session, `stale=0`.
- Cache: after every reboot, `[wx] cache: <stn> obs=…` is loaded at 3.61 s (before the face at 3.72 s). After the boot
  fetch, `cache=0` (fresh).
- Certificate failure keeps the last good report (see T017).
- **Pending (user):** network unreachable ×10 (SC-004) and the reboot-without-network variant of SC-005; Wi-Fi
  forgotten → NO WIFI. These need the router's internet cut, and consent for credential changes.

## Q7: station (US4)
- `ad wx set LTFM` → fetch Ok 6.2 s later (LTFM VFR). Reboot → `wxstn=LTFM` kept, the LTFM cache shown, then a fresh
  fetch.
- `ad set wxstn ltac` (lowercase, generic settings command) → `LTAC`, fetch Ok 4.3 s later.
- `ad wx set lt1c` → `ERR station must be 4 letters A-Z`.
- **Pending (user):** Settings → Weather station picker by touch, timed (SC-007 <30 s).

## Q8: display (US5)
- Implemented: compass (north-up rose, arrow from the wind bearing to the centre, `CALM` / `VRB` text, sector arc),
  tiles, the raw text box (wrapping, scrollable), and the REFRESH button inside the rounded safe area.
- **Pending (user):** visual check with `ad wx decode "<text>" show` for VRB+gust+sector, SM fractions, negative
  temperatures, CAVOK, calm and the longest corpus report, plus the arrow at 000/090/180/270.

## User review on the watch (2026-11-11)
- **WX page:** the raw METAR box ran under REFRESH and hid the last line. Fixed by pinning REFRESH to the bottom
  (outside the flex flow) and sizing the raw box from absolute coordinates so it ends 4 units above the button. The
  box now spans y=301..412 and the button y=420..487.
- **REFRESH** made 15 % smaller at the user's request: 51 % x S(40) ≈ 209 x 68 px. The theme's S(48) touch minimum is
  overridden for this button only.
- **Readable fields:** `QNH 1020 hPa`, `TEMP 8°C, dew 4°C` (beside the compass), and `CLOUDS Few 3,500 ft, broken
  18,000 ft` (full width). New pure helpers `Metar::formatCloudsText` / `formatTemp`, host-tested.
- **PFD (S1 component, fixed here):**
  - the hour/minute tape digits went from DSEG7 40 → 28 px, so they no longer touch the green box border;
  - the MAX G / FLT NM labels are now aligned towards the centre, away from the curved glass;
  - "FLIGHT NM" was shortened to "FLT NM" (it wrapped and was clipped).

  PFD after the change: median 30 fps (min 28), heapmin 59.4 KB.
- The user confirmed the WX page (fields, compass, box, button) and the PFD changes look right (T031 visual check).

## Q9: 24-h soak (SC-006)
- Pending (T036). It can run together with S2 T038.
