# Data Model: Alert Manager & Haptics (Slice S4)

## 1 · Levels, ids, table (`src/core/Alerts.h`, pure)
- `enum class AlertLevel : uint8_t { Advisory = 1, Caution = 2, Warning = 3 }` (higher = more important).
- `enum class AlertId : uint8_t` keeps the S0 `CautionId` numbering, so log lines stay comparable:

  | Id | Value | Level | Text (default) | Pattern | Repeat |
  |---|---|---|---|---|---|
  | `PeriphFirst + n` | 0..12 | Caution | "`<PERIPH>` FAIL" | Caution | once |
  | `SettingsReset` | 32 | Caution | "SETTINGS RESET" | Caution | once |
  | `SettingsStorageError` | 33 | Caution | "SETTINGS STORAGE ERROR" | Caution | once |
  | `SafeMode` | 34 | Caution | "SAFE MODE" | Caution | once |
  | `ClockNotSet` | 35 | Caution | "CLOCK NOT SET" | Caution | once |
  | *(36, LOW FUEL: retired, replaced by Stall)* | | | | | |
  | `TimeUnsync` | 37 | Advisory | "TIME UNSYNC" | Advisory | once |
  | `Stall` | 40 | Warning | "STALL" | StickShaker | 2000 ms until ack |
  | `PullUp` | 41 | Warning | "PULL UP" | StickShaker | 2000 ms until ack |
  | `Test1` / `Test2` / `Test3` | 60–62 | set by the command | set by the command | by level | by level |

- `struct AlertDef { AlertId id; AlertLevel level; Pattern pattern; uint16_t repeatMs; uint8_t voiceClip; }`.
  `voiceClip` 0 = none; S5 fills it in.
- `const AlertDef* alertDef(AlertId)`: a constexpr table lookup (peripheral ids are resolved by range).

## 2 · Alert set (`src/core/AlertSet.h`, pure)

```text
struct Alert { AlertId id; AlertLevel level; char text[25]; bool acked; uint32_t raisedSeq; };
class AlertSet {           // capacity kCap = 24
  RaiseResult raise(id, level, text);   // Added | Updated | Rearmed | Same | Full
  bool clear(id);
  uint8_t ackAll();                     // number newly acknowledged
  bool ack(id);                         // acknowledge one alert (used for a de-escalated STALL); false if absent
  bool anyUnacked() const; bool anyUnackedWarning() const;
  size_t sorted(Alert* out, size_t max) const;   // level desc, raisedSeq asc
  size_t count() const;
};
```

- **Added:** a new id → `acked = false`, `raisedSeq = ++seq`.
- **Same:** the id exists with the same level and text → no change, no pattern.
- **Updated:** the text changed, same or lower level → text replaced; `acked` and `raisedSeq` unchanged; **no pattern**
  (spec Edge Cases: only a higher level re-arms).
- **Rearmed:** the level went up → `acked = false`; `raisedSeq` unchanged; the pattern plays.
- **Full:** at capacity, the new alert is dropped and an error is logged. Capacity 24 is ≥ the spec's 16.

## 3 · Battery alarm (`src/core/BatteryAlarm.h`, pure)
- States `Normal`, `Stall`, `PullUp`.
- `update(pct, charging, valid) → Transition { none | raiseStall | raiseStallAcked | raisePullUp | clearAll }`.

| From | Condition | To | Action |
|---|---|---|---|
| any | `!valid` | unchanged | none |
| any | charging | Normal | clearAll (clear Stall, PullUp) |
| Normal | pct < 15 (and > 5) | Stall | raiseStall |
| Normal / Stall | pct ≤ 5 | PullUp | raisePullUp (clear Stall) |
| PullUp | pct ≥ 7 and < 17 | Stall | raiseStallAcked (clear PullUp) |
| PullUp / Stall | pct ≥ 17 | Normal | clearAll |

## 4 · Haptic policy (`src/core/HapticPolicy.h`, pure)
- `enum class Pattern : uint8_t { None, Click, Advisory, Caution, StickShaker }`.
- `enum class Intensity : uint8_t { Off, Low, High }`.
- `struct Seq { uint8_t n; uint8_t w[8]; uint16_t lengthMs; uint8_t prio; }`. Prio: Click 1, Advisory 2, Caution 3,
  StickShaker 4.
- `bool select(Pattern, Intensity, bool quiet, Seq& out)`: the effect table from research R2.
  - StickShaker always returns true.
  - Advisory/Caution return false when Off or quiet.
  - Click returns false when Off.
- `bool shouldStart(uint8_t newPrio, uint8_t curPrio, uint32_t curEndMs, uint32_t nowMs)`:
  `nowMs >= curEndMs || newPrio >= curPrio`. A click is never queued.

## 5 · Snapshot (`src/services/AlertManager.h`)

```text
struct AlertSnapshot { uint8_t n; Alert items[16]; bool anyUnacked; uint8_t total; uint32_t seq; };
```

`items` are sorted (master first). `total` may exceed 16; the band shows `+(total-1)`.

## 6 · Settings (schema v5)

| Key | NVS | Type | Default | Range | Editable |
|---|---|---|---|---|---|
| `HapticLvl` | `hapticlvl` | U8 | 1 (Low) | 0..2 | yes |
| `Quiet` | `quiet` | Bool | 0 | 0..1 | yes |

The migration 4→5 is additive (persist defaults).

## 7 · Events (EventBus)
- `AlertRaised` (a = AlertId), `AlertCleared` (a = AlertId), `AlertAcked`.
- These replace `CautionChanged`; the UI listens to them and to the snapshot seq.
