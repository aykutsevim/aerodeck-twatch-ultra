# Data Model: G-Force & Flight Recorder (Slice S6)

## 1 · Day record (`src/core/DayLog.h`, pure)
| Field | Type | Notes |
|---|---|---|
| `day` | `int32_t` | local day number (days since 1970-01-01 local); `kUndated = -1` while the clock was never valid; `kEmpty = INT32_MIN` for a skipped day slot |
| `steps` | `uint32_t` | steps counted that day |
| `maxGc` | `uint16_t` | Max G in centi-g (100 = 1.00 g); 0 = none yet |
| `maxGMin` | `int16_t` | local minute of day (0..1439) of Max G; −1 = none |

NM is never stored: `nm = steps × strideCm / 185200` (§4).

## 2 · Day log (`core::DayLog`, pure)
- `DayRecord today; DayRecord history[7];` (index 0 = yesterday / newest).
- `void addSteps(uint32_t n)`.
- `bool notePeak(uint16_t gc, int16_t minuteOfDay)`: raises Max G only if higher; returns true when raised.
- `RollResult advance(bool clockValid, int32_t localDay)` → `{bool rolled; uint8_t emptyDaysInserted; bool dated;}`:
  - invalid clock → nothing;
  - `today.day == kUndated` → `today.day = localDay`, `dated = true`;
  - `localDay > today.day` → history shifts by `1 + min(localDay − today.day − 1, 7)` (the finished day, then empty
    days), oldest dropped; today = `{localDay, 0, 0, −1}`;
  - otherwise nothing (backward clock / time zone change).
- `const DayRecord* dayAt(int idx)`: 0 = today, 1..6 = previous days for the chart (by date: a missing date shows
  empty).
- `static int32_t localDayOf(int64_t utcSec, int16_t tzOffsetMin)` (floor division).
- `uint8_t weekday(int32_t localDay)` (0 = Monday; 1970-01-01 was a Thursday).

## 3 · Step accumulator (`core::StepAccumulator`, pure)
- `uint32_t last = 0; uint32_t lastMs; bool primed`.
- `uint32_t feed(uint32_t raw, uint32_t nowMs, bool& rejected)`:
  - raw ≥ last → delta = raw − last;
  - raw < last → sensor restarted → delta = raw;
  - delta > 6 × elapsed_s + 20 → `rejected = true`, delta = 0, `last` re-baselined to raw;
  - updates `last`, `lastMs`; returns delta.

## 4 · Flight math (`core::FlightMath`, pure)
- `double nm(uint32_t steps, uint8_t strideCm)` = steps × strideCm / 185200.
- `void formatNm(double nm, bool ok, char* out, size_t n)`: "--" / "0.00" / "3.42" / "12.4".
- `uint16_t toCentiG(float g, float rangeG, bool& saturated)` (clamped to range).
- `void formatG(uint16_t gc, bool ok, bool saturated, char* out, size_t n)`: "--" / "1.0" / "2.7" / "16.0+".

## 5 · Persistence blob (NVS namespace `rec`, key `day`)
| Offset | Field |
|---|---|
| 0 | `uint8_t version = 1` |
| 1 | `uint8_t count = 8` (today + 7) |
| 2.. | 8 × packed `DayRecord` (4 + 4 + 2 + 2 = 12 bytes) |
| 98 | `uint16_t crc16` (CCITT over bytes 0..97) |

`core::RecBlob::encode(const DayLog&, uint8_t* buf)` / `decode(const uint8_t*, size_t, DayLog&)` → false on
version / length / CRC mismatch (pure, host-tested).

## 6 · Settings (schema v7)
| Key | NVS | Type | Default | Range | Editable |
|---|---|---|---|---|---|
| `Stride` | `stride` | U8 (cm) | 75 | 30..150 | yes |

## 7 · RecorderSnapshot (`src/services/RecorderService.h`)
```text
bool stepsOk, accelOk;        // sources available (set in onImuReady)
float gNow;                   // latest |a| in g (ad rec g_now; R2 scale check)
uint32_t steps; uint16_t maxGc; int16_t maxGMin; bool maxGSat;
uint8_t strideCm; float rangeG;
DayRecord chart[7];           // [0] = today, [1..6] previous days by date (empty records where missing)
int32_t today;                // local day number or kUndated
uint32_t lastSaveMs; uint32_t saves; uint32_t rejectedJumps;
uint32_t seq;
```
Single writer: the svc task (Snapshot seqlock).

## 8 · State transitions (per svc second)
```text
poll steps → StepAccumulator → DayLog.addSteps
poll peak  → toCentiG → DayLog.notePeak(minute of day)
clock      → DayLog.advance(valid, localDay) → on roll: save
dirty && 10 min since save → save
publish snapshot
esp_restart() → shutdown handler → save
```
