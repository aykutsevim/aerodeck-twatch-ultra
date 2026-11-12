# Data Model: GPWS Audio (Slice S5)

## 1 · Clips (`src/core/Alerts.h`, extended)

| Clip | File (`/audio/…`) | Content | Used by |
|---|---|---|---|
| `MasterCaution` | `master_caution.wav` | "Master caution" | every CAUTION (S5) |
| `MasterWarning` | `master_warning.wav` | "Master warning" | WARNING without its own clip |
| `Stall` | `stall.wav` | "Stall. Stall." | STALL |
| `PullUp` | `pull_up.wav` | "Pull up. Pull up." | PULL UP |
| `BingoFuel` | `bingo_fuel.wav` | "Bingo fuel" | S8 timer end |
| `Minimums` | `minimums.wav` | "Minimums" | generic alarm; S4 Test1..3 |
| `Traffic` | `traffic.wav` | "Traffic. Traffic." | S9 radar |
| `Chime` | `chime.wav` | two-tone chime (synthesized) | hourly chime, "Play sample" |

- `const char* clipFile(Clip)` and `clipFromName(const char*)` (names: `master_caution`, …).
- `Clip calloutFor(AlertId, AlertLevel)`, evaluated in this order:
  1. level Advisory → None (always, even if the alert has its own clip; FR-005);
  2. `AlertDef::voiceClip` if set (e.g. Test1..3 → Minimums at Caution/Warning);
  3. otherwise MasterCaution (Caution) or MasterWarning (Warning).

## 2 · WAV header (`src/core/Wav.h`, pure)
- `struct WavInfo { uint32_t rate; uint16_t channels; uint16_t bits; uint32_t dataOffset; uint32_t dataBytes; }`.
- `bool parse(const uint8_t* buf, size_t len, WavInfo&)`:
  - **Accepts:** RIFF/WAVE with `fmt ` (PCM = 1) and a `data` chunk, with unknown chunks (LIST, …) skipped.
  - **Rejects:** non-PCM, bits ≠ 16, channels ≠ 1, rate outside 8000..48000, a truncated header, or a data size
    larger than the declared file size.
- Needs up to 256 header bytes.

## 3 · Audio math (`src/core/AudioMath.h`, pure)
- `int16_t gainQ15(uint8_t volumeStep)`: steps 0..4 → {0, 9830, 16384, 23170, 32767}
  (0 / 0.30 / 0.50 / 0.71 / 1.0; raised from {0, 0.18, 0.35, 0.6, 1.0} after the T016 listening check).
- `void applyGain(int16_t* s, size_t n, int16_t q15)` with saturation.

## 4 · Sound queue (`src/core/SoundQueue.h`, pure)
- `enum class SoundPrio : uint8_t { Sample = 1, Chime = 2, Caution = 3, Warning = 4 }`.
- `struct SoundReq { Clip clip; SoundPrio prio; uint32_t atMs; }`.
- `class SoundQueue` (capacity 6):
  - `push(req)` → `Queued | Duplicate | Full` (Full drops the lowest priority if the new one is higher);
  - `pop(nowMs, out)`: the highest priority, oldest first; stale entries are dropped
    (`maxAgeMs(prio)`: Chime 2000 ms, all others 5000 ms; `millis` wrap safe);
  - `purge(minPrio)`;
  - `size()`.
- `bool shouldPreempt(SoundPrio playing, SoundPrio incoming)`: incoming > playing.

## 5 · Policies (`src/core/AudioPolicy.h`, pure)
- `bool inQuietHours(int localHour, uint8_t start, uint8_t end)`: `[start, end)`, wrapping; start == end → false.
- `bool allow(SoundPrio p, bool quiet, uint8_t volume, bool inQuietHours)`:
  - false if quiet or volume == 0;
  - if in quiet hours, only Warning is allowed;
  - otherwise true.
- `ChimeDecision chimeDue(int64_t localSec, bool clockValid, bool enabled, bool quiet, uint8_t volume,
  uint8_t qhStart, uint8_t qhEnd, int64_t lastHourKey)` → `{bool due; int64_t hourKey;}`:
  - `hourKey = floor(localSec / 3600)`;
  - due iff: the clock is valid, enabled, not quiet, volume > 0, `localSec % 3600 < 5`,
    `hourKey != lastHourKey`, and not in quiet hours (for that hour).

## 6 · Settings (schema v6)

| Key | NVS | Type | Default | Range | Editable |
|---|---|---|---|---|---|
| `Volume` | `volume` | U8 | 3 | 0..4 | yes |
| `Chime` | `chime` | Bool | 1 | 0..1 | yes |
| `QhStart` | `qhstart` | U8 | 22 | 0..23 | yes |
| `QhEnd` | `qhend` | U8 | 7 | 0..23 | yes |

## 7 · AudioService state (`src/services/AudioService.h`)
- `AudioStatus { bool ampOn; Clip playing; uint8_t queued; uint32_t played; uint32_t skipped; }` (read by `ad audio`).
- `enum class PlayResult : uint8_t { Queued, Suppressed, Duplicate, Full, NoClip }`, returned by
  `AudioService::play(Clip, SoundPrio)`, plus the suppression reason (`quiet`, `volume off`, `quiet hours`) for
  the console reply.
- **Output failure:** if the speaker is not `Ok` or `audioBegin` fails, the request and the rest of the queue are
  dropped (`skipped += n`), the lease is released, and one `[aud] output unavailable` line is logged per boot. The
  task waits for the next request (no retry loop).
- **Events:** none new. AlertManager calls `AudioService::play/stopAlerts` directly through its thread-safe API.
