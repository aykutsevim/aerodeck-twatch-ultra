# Quickstart & Validation: METAR Weather Page (S3)

**Prerequisites:** as in S2:
- PlatformIO via `python -m platformio`;
- WinLibs GCC on PATH for the native tests;
- the watch on COM3, with the serial port opened with DTR/RTS low;
- **Wi-Fi configured** (S2) on a network with internet access.

For decode checks, keep a browser handy: `https://aviationweather.gov/api/data/metar?ids=LTAC&format=raw`.

## Q1. Build & tests
```powershell
pio test -e native     # all suites + test_metar (≥20 real reports), test_http_resp, test_settings (v4)
pio run -e twatch_ultra -t upload --upload-port COM3
pio run -e twatchs3
```

## Q2. Migration and default (FR-016)
After flashing over S2 settings:
- `ad set list` shows `wxstn=LTAC`;
- the S2 values (`wifissid`, `tzid`, `syncmin`) are unchanged;
- `ad wx` → `stn=LTAC`.

## Q3. Boot fetch (US1, SC-002, FR-001, FR-003)
```text
ad reboot   → log: [net] sync: Ok … then [wx] fetch: Ok obs=…Z cat=…; then rail wifi OFF (one radio session)
ad wx       → has=1 stale=0 last=Ok
```
- Swipe to WX: every field matches the provider's raw text for the same observation.
- Repeat 10 boots (SC-002: fresh report within 30 s, 10/10).
- Record the session length (`rail wifi ON` → `OFF`) and the fetch duration (`ms=`).

## Q4. Decoder on device and SC-001
- `ad wx decode "<text>"` for 5 reports taken live from different stations: the fields agree with a manual decode
  and with the off-device test results.
- Together with the native corpus (≥20 reports, all passing), this gives SC-001 (20/20).

## Q5. Refresh, coalescing, performance (US3, SC-003, FR-015, FR-019)
- `ad set overlay 1`, then `ad att sweep on`. On WX, tap **REFRESH** 10 times, waiting for each result:
  - `REFRESHING…` appears immediately (≤100 ms, observed by eye/video);
  - the result arrives ≤20 s later;
  - swiping to the PFD during a fetch stays smooth.
- Double-tap REFRESH: only one `[wx] fetch` log line.
- `ad stat` during the fetches: FPS median ≥28, and **heapmin ≥40 KB** (research R3). Record the values.
- If heap <40 KB, apply the R3 fallback and re-measure.

## Q6. Failures and cache (US2, SC-004, SC-005, FR-011–FR-014)
1. `ad wx set ZZZZ` → `[wx] fetch: NoReport`; the page shows `ZZZZ`, `NO DATA`, `NO REPORT`.
2. `ad wx set LTAC` → Ok again.
3. **Network unreachable:** unplug the router's WAN (or switch off the AP after the sync), then `ad wx fetch` ×10.
   Each attempt: `NetFail` ≤15 s, the old report stays, `NET FAIL` is shown, and `rail wifi OFF` follows. (This needs
   the user. Changing the credentials is not required.)
4. `ad wx age 6000` → `STALE` appears within 60 s. `ad wx age -1` → it disappears.
5. With the network still unreachable, `ad reboot` → the WX page shows the cached LTAC report within 5 s of the watch
   face appearing (SC-005), marked STALE if it is old, plus `NET FAIL` after the boot attempt.
6. `ad wifi forget` (only with the user's consent; re-provision afterwards) → `NO WIFI`, and the cached report stays.
   Alternative without consent: verify the `NoWifi` path in the native/host logic review only.
7. Clock not set: `ad time set 2020-01-01T00:00:00Z` with the network unreachable → STALE (the report can't be judged
   fresh).

## Q7. Station change (US4, SC-007)
- Settings → Weather station → set `LTFM` → OK. Time it: under 30 s. The WX page shows LTFM within 20 s.
- `ad reboot` → `wxstn=LTFM` is kept.
- `ad wx set ltac` → `OK stn=LTAC` (case-insensitive). `ad wx set LT1C` → `ERR`.

## Q8. Display checks (US5, edge cases)
- Use `ad wx decode` with long and edge reports to check the layout, e.g.:
  - `METAR KXYZ 111256Z VRB03G15KT 250V310 1 1/2SM BKN004 OVC010 M12/M15 A2992`;
  - a CAVOK report;
  - an `00000KT` report.

  For each, check that the compass shows `VRB`+sector / `CALM` correctly, that nothing is clipped by the rounded
  corners, and that the raw text scrolls.

  The decode hook only prints; to *render* a test report, use `ad wx decode … show`, which loads it into the
  snapshot as `fromCache=0, obsUtc=now`, RAM only.

## Q9. 24-h soak (SC-006)
- Leave the watch on the network for 24 h with serial logging:
  - 24/24 `[wx] fetch: Ok`;
  - `rail wifi OFF` after each;
  - no displayed report older than 90 min without STALE.
- This can run together with the S2 soak (T038).
