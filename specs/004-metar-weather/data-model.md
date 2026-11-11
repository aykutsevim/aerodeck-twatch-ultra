# Data Model: METAR Weather Page (Slice S3)

## 1 · `core::Metar::Report` (pure, `src/core/Metar.h`)

| Field | Type | Missing value | Notes |
|---|---|---|---|
| `station` | `char[5]` | `""` | `[A-Z]{4}` |
| `day, hour, minute` | `int8_t` | `-1` | from `DDHHMMZ` |
| `nil` | `bool` | | report body is `NIL` |
| `windDir` | `int16_t` | `-1` | 0–360 °, `kWindVrb = -2` |
| `windSpdKt` | `int16_t` | `-1` | 0 + `windDir == 0` → calm |
| `windGustKt` | `int16_t` | `-1` | |
| `windVarFrom, windVarTo` | `int16_t` | `-1` | `dddVddd` sector |
| `cavok` | `bool` | | |
| `visM` | `int32_t` | `-1` | prevailing visibility in metres (SM converted ×1609.344, rounded); `9999` → 10000 |
| `visText` | `char[12]` | `""` | display form as reported: `"10 km+"`, `"4000 m"`, `"1 1/2 SM"`, `"6+ SM"`, `"<1/4 SM"`, `"CAVOK"` |
| `visIsSm` | `bool` | | |
| `clouds[6]` | `{Cover cover; int16_t baseFt; bool cb;}` | `nClouds = 0` | `Cover {Few, Sct, Bkn, Ovc, Vv}`; base `-1` = unknown (`///`) |
| `noCloud` | `enum {Unset, Nsc, Ncd, Skc, Clr}` | `Unset` | |
| `tempC, dewC` | `int8_t` | `kNoTemp = -128` | |
| `qnhHpa` | `int16_t` | `-1` | `A` group converted |
| `qnhWasInHg` | `bool` | | display may add the original |

Derived (pure functions, `Metar.h`):
- `ceilingFt(const Report&) -> int32_t`: the lowest BKN/OVC/VV base. `kNoCeiling = INT32_MAX` means unlimited.
  `-2` means unknown (no cloud info and not CAVOK/NSC/NCD/SKC/CLR).
- `category(const Report&) -> Category {Unknown, Vfr, Mvfr, Ifr, Lifr}` (spec FR-007, research R6).
- `resolveObsTime(day, hour, minute, nowUtc) -> int64_t` (0 if `nowUtc <= 0` or day/hour/minute missing) (R5).
- `isStale(obsUtc, nowUtc) -> bool`: `obsUtc == 0 || nowUtc <= 0 || nowUtc - obsUtc > 90*60` (FR-011).
- `formatWind(const Report&, char*, n)`: e.g. `"270° 15G25 kt"`, `"VRB 03 kt"`, `"VRB 250-310° 06 kt"`, `"CALM"`,
  `"--"`.
- `formatClouds(const Report&, char*, n)`: e.g. `"FEW016 BKN030"`, `"NSC"`, `"CAVOK"`, `"VV002"`, `"--"`.
- `pickNewest(body, len, wantStation, out, cap) -> bool`: selects the first line whose station matches, since the
  provider returns newest first. It trims CR/LF and caps the line at 255 characters.

Validation (FR-020): every numeric field is range-checked as in research R6. Unknown tokens are skipped, and
decoding never reads past the terminating NUL. Input is capped at 255 characters.

## 2 · `core::HttpResp` (pure, `src/core/HttpResp.h`)
`parse(const char* buf, size_t len, bool closed) -> Result` *(implementation: a three-state result instead of
`bool` + `complete`)*. Result fields:
- `state`: `Incomplete` (read more), `Complete` (headers ended and the body is all there, or the status has no body),
  `Error` (malformed, over the limits, or the connection closed early, e.g. before `Content-Length` bytes arrived);
- `status`;
- `contentLength` (−1 when absent; without it, the body runs until the connection closes, ≤ 2048 B);
- `chunked` (reported as `Complete`; the caller treats it as `ServerError`);
- `bodyOff`, `bodyLen`.

It rejects: no status line, headers > 1536 B, `Content-Length` > 2048 or with non-digits.

## 3 · `WeatherSnapshot` (service → UI, `src/services/WeatherService.h`)

| Field | Type | Notes |
|---|---|---|
| `station` | `char[5]` | currently configured station |
| `hasReport` | `bool` | a decoded report for `station` is present (fresh or cached) |
| `report` | `core::Metar::Report` | ~120 B |
| `raw` | `char[256]` | raw METAR line |
| `obsUtc` | `int64_t` | 0 = unknown |
| `fetchedUtc` | `int64_t` | 0 = never |
| `fromCache` | `bool` | loaded at boot and not yet refreshed |
| `busy` | `enum WxBusy {Idle, Pending, Fetching}` | |
| `lastResult` | `WxResult {None, Ok, NoWifi, NetFail, NoReport, ServerError}` | R10 |
| `lastAttemptUtc` | `int64_t` | |
| `seq` | `uint32_t` | increments on every publish |

The size is about 450 B, published through `Snapshot<WeatherSnapshot>` (seqlock, S0). STALE isn't stored: the page
computes it from `ClockSnapshot` (FR-011).

**State transitions (`busy`):**
- `Idle → Pending` on `requestWeather()`;
- `Pending → Fetching` when the net task starts the weather step;
- `Fetching → Idle` with a `lastResult`.

A sync's weather step goes `Idle → Fetching → Idle`.

**Result handling:**
- **Ok** (newer or equal `obsUtc`): replace the report and raw text, clear the error, write the cache if `obsUtc`
  changed, and post `WeatherUpdated`.
- **Not Ok**: keep the report and set `lastResult` (FR-012).
- **Station changed mid-fetch:** the result is discarded (`lastResult = None`) and a new fetch is requested.

## 4 · Settings (schema v4)

| Key | NVS key | Type | Default | Range / validation | Editable | Notes |
|---|---|---|---|---|---|---|
| `WxStation` | `wxstn` | `Str` | `"LTAC"` | exactly 4, `[A-Za-z]` → stored uppercase | yes | new in S3 (FR-016) |

- `SettingDef` gains `const char* sdef` (string default; `nullptr` = empty).
- `validateString(WxStation, s)` returns false unless the input is 4 letters.
- **Migration 3 → 4:** additive only. The existing "key added by the new schema → persist default" path writes
  `wxstn=LTAC`.
- **Restore defaults** resets the station to `LTAC` and clears the weather cache. Its `SettingsChanged(WxStation)`
  event makes WeatherService clear the in-RAM report and re-fetch, even if the station was already LTAC.

## 5 · Weather cache (NVS blob, not a setting)
Key `wxcache` in namespace `aerodeck`, written through `SettingsStore::setBlob()`:

```text
u8  version = 1
char station[5]
i64 fetchedUtc
i64 obsUtc
u16 rawLen (≤255)
char raw[rawLen]
```

Rules:
- An unknown version, a wrong length, or `station ≠ wxstn` → the cache is ignored and erased.
- It is written only after `Ok` with a changed `obsUtc`.
- It is erased on a station change, on `ad wx clear`, and on restore defaults.

## 6 · NetService changes (S2 entities)
- `NetBusy` gains `Weather`.
- `Cmd` gains `Weather`.
- `NetService::requestWeather()` sets an atomic pending flag and posts once (R4).
- `NetResult` is unchanged; weather results live in `WeatherSnapshot.lastResult` (FR-004 independence).

## 7 · Events
- `WeatherUpdated` (EventBus, already named in AD-SAS-001 §5.2) is posted after every completed weather attempt. The
  page also polls `seq`.
- `SettingsChanged(WxStation)` triggers a cache clear and a fetch.
