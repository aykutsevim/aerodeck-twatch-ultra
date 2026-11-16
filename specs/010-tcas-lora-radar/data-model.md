# Data Model: TCAS LoRa Radar (Slice S9)

All `core::` types are pure and host-tested.

## 1. Settings v10 (`core::SettingDefs`, NVS `aerodeck`)

| Key | Type | Default | Range | Meaning |
| :-- | :-- | :-- | :-- | :-- |
| `tcas` | Bool | 0 | 0..1 | TCAS on/off (persisted, survives eject) |
| `callsign` | Str | "" → `AD-xxxx` (last 4 hex digits of the node id) | 1..8 chars `[A-Z0-9-]` | Callsign |
| `tcasint` | U16 | 30 | 15..300 | Beacon interval (s) |
| `tcasage` | U16 | 300 | 60..1800 | Contact age-out (s) |
| `lorareg` | U8 | 0 | 0..1 | Region: 0 EU868, 1 US915 |
| `lorapwr` | U8 | 10 | 2..20 | TX power dBm (clamped by the region) |
| `lorasf` | U8 | 9 | 7..12 | Spreading factor |
| `lorabw` | U8 | 0 | 0..2 | Bandwidth index into the region's list (EU868: 125/250; US915: 125/250/500); out-of-list → 0 |

`kSchemaVersion` = 10; migration v9 → v10 adds the defaults.

## 2. `core::LoraRegion`

```text
struct RegionLimits { const char* name; float freqMinMHz, freqMaxMHz, freqDefaultMHz;
                      int8_t powerMin, powerMax; uint16_t dutyPerMille; uint16_t maxDwellMs;
                      float bwKHz[3]; uint8_t bwCount; }
EU868: 869.40–869.65 MHz (sub-band g3), default 869.525, power 2..14 dBm, duty 10 ‰ (1 %, self-imposed), dwell 0
       (none), BW {125, 250}
US915: 902.3–914.9 MHz, default 903.9, power 2..20 dBm, duty 10 ‰ (self-imposed), dwell 400 ms, BW {125, 250, 500}
RadioProfile resolve(region, powerDbm, sf, bwIdx)   // clamps every field; reports what was clamped
```

## 3. `core::BeaconCodec` (SAS §7.2, v1)

```text
struct Beacon { uint32_t nodeId; uint8_t seq; char callsign[9]; uint8_t battPct; bool hasPos; int32_t lat24, lon24; }
size_t encode(const Beacon&, uint8_t* out)      // 18 B (no position) or 24 B
bool decode(const uint8_t* buf, size_t n, Beacon& out)
     // rejects: n not 18/24, magic != AD 01, type != 01, callsign not [A-Z0-9- ] after uppercasing,
     //          battPct > 100, hasPos flag vs length mismatch
bool isMeshtastic(const uint8_t*, size_t)       // FR-TCS-007 hook: always false in S9
```

## 4. `core::LoraAirtime`

`uint32_t airtimeMs(uint8_t sf, float bwKHz, uint8_t crDenom /*5..8*/, uint16_t preamble, size_t payload,
bool explicitHeader, bool crc)`
- The Semtech AN1200.13 formula, with low-data-rate optimisation when the symbol time is ≥ 16 ms.
- SF9/BW125/CR5/preamble 8/18 B ≈ 185 ms.

## 5. `core::DutyCycle`

```text
DutyCycle(uint32_t windowMs = 3600000, uint32_t budgetMs = 36000)
bool canSend(int64_t nowUtcMs, uint32_t airMs) const   // sum(window) + air <= budget
void record(int64_t nowUtcMs, uint32_t airMs)
uint32_t usedMs(int64_t nowUtcMs) const
int64_t nextAllowedMs(int64_t nowUtcMs, uint32_t airMs) const   // earliest time canSend becomes true
size_t save(uint8_t* out, size_t cap) const           // magic + records + CRC-16 (RTC_NOINIT persistence)
bool restore(const uint8_t* in, size_t n, int64_t nowUtcMs)   // false on bad magic/CRC; future records → now
```

Ring of 256 records (more than 240 beacons per hour at 15 s). Times are UTC ms from the system clock, so the ring
survives eject and soft reset in RTC memory (research R4).

## 6. `core::TcasTable`

```text
struct Contact { uint32_t nodeId; char callsign[9]; float rssiEma; int8_t lastRssi; int8_t lastSnrQ4;
                 uint8_t battPct; uint32_t lastSeenMs; uint8_t ring; uint16_t bearingDeg; bool inInner;
                 bool simulated; }
constexpr int kMax = 16; constexpr float kAlpha = 0.3f; constexpr int kRings = 4;
int  update(const Beacon& b, int16_t rssi, int8_t snrQ4, uint32_t nowMs, bool simulated, bool& enteredInner)
void age(uint32_t nowMs, uint32_t timeoutMs)        // removes stale contacts
static uint8_t ringFor(float rssi)                  // clamp((-40 - rssi) / 15, 0, 3)
static uint16_t bearingFor(uint32_t nodeId)         // stable hash → 0..359
```

**Rules:**
- A new contact: `rssiEma` = rssi.
- `enteredInner` is true when the ring becomes 0 and `inInner` was false. `inInner` follows `ring == 0`; leaving
  re-arms it.
- When the table is full, the contact with the oldest `lastSeenMs` is replaced.

## 7. `TcasSnapshot` (svc → page, console)

`on, radioOk, region, freqMHz, powerDbm, sf, callsign, nodeId, interval, beaconsSent, beaconsDelayed, beaconsSkipped,
rxPackets, rxForeign, rxCrcErrors, dutyUsedMs, n, contacts[16] (copy), seq`

## 8. Alerts

| Id | Name | Level | Pattern | Clip | Text |
| :-: | :-- | :-- | :-- | :-- | :-- |
| 44 | `traffic` | Advisory | Advisory | `Traffic` | "TRAFFIC" |

`calloutFor`: an advisory returns its explicit clip if it has one, otherwise `None` (all other advisories stay
silent). `ScreenPolicy::onAlert(level, nowMs, speaks)`: Caution/Warning wake as before; an Advisory wakes only when
`speaks` is true. The `AlertRaised` event `b` carries the level in bits 0–7 and `speaks` in bit 8.
