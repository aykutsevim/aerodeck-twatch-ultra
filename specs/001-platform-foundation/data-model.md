# Data Model: Platform Foundation (S0)

## 1. Setting

| Field | Type | Notes |
| :--- | :--- | :--- |
| `key` | string ≤15 chars | NVS key (NVS limit: 15 chars) |
| `type` | enum {Bool, U8, U16, I32, Str} | |
| `def` | value | Default |
| `min`, `max` | numeric | Ignored for Bool/Str. Str uses `max` as max length |
| `secret` | bool | Never logged, shown masked |
| `editable` | bool | Shown on Settings page if true |

**Validation**: on load, a value outside `[min,max]` (or a wrong type, or a missing key) falls
back to `def`, and the fallback is logged at WARN (key only).

**S0 settings registry** (namespace `aerodeck`):

| Key | Type | Default | Range | Editable | Purpose |
| :--- | :--- | :--- | :--- | :---: | :--- |
| `schema` | U16 | 1 | — | no | Settings format version |
| `bright` | U8 | 3 | 0–4 (5 steps → 20/40/60/80/100 % of max) | yes | Display brightness |
| `overlay` | Bool | false | — | yes | Diagnostics overlay |
| `loglvl` | U8 | 2 | 0 ERROR, 1 WARN, 2 INFO, 3 DEBUG | yes | Diagnostic level |
| `simfail` | U16 | 0 | bitmask (see §3) | no (serial only) | Simulated peripheral failures at boot |

Later slices append keys and bump `schema`.

**Schema migration state machine**
```
stored schema == current  → load
stored schema <  current  → run migrations stored+1..current → write schema → load
stored schema >  current  → clear() → write defaults → raise caution "SETTINGS RESET"
key `schema` missing      → first boot: write defaults (no caution)
NVS open/read error       → use in-memory defaults → caution "SETTINGS STORAGE ERROR"
```

## 2. Page

| Field | Type | Notes |
| :--- | :--- | :--- |
| `id` | enum PageId {Home, PlaceholderA, PlaceholderB, Settings} | Order = ring order |
| `title` | const char* | Shown in page indicator |
| `screen` | lv_obj_t* | Created lazily |
| `state` | enum {NotCreated, Hidden, Visible} | |

**Lifecycle transitions**
```
NotCreated --onCreate--> Hidden --onEnter--> Visible --onExit--> Hidden
Visible: onTick(dt) called from UI loop at ≤ 10 Hz
```
`onExit` MUST release every `RailLease` the page holds.

**Navigation state (PageManager)**: `current`, `lastNonHome` (init = PlaceholderA),
`transitionBusy` (bool), `pendingHome` (bool, set by a button press during a transition).

## 3. Peripheral status

| Field | Type | Notes |
| :--- | :--- | :--- |
| `id` | enum Periph {Pmu, Display, Touch, Rtc, Imu, Haptics, Lora, Nfc, Gnss, SdCard, Mic, Speaker, Expander} | |
| `present` | bool | Board has it (capability) |
| `state` | enum {Ok, Failed, Absent, SimulatedFail} | |
| `optional` | bool | false for Pmu, Display, Touch |

`simfail` bit ↔ peripheral: bit0 Lora, bit1 Nfc, bit2 Gnss, bit3 SdCard, bit4 Mic, bit5 Haptics,
bit6 Imu, bit7 Rtc. Core peripherals cannot be simulated as failed.

## 4. Caution (S0 minimal form)

| Field | Type | Notes |
| :--- | :--- | :--- |
| `id` | enum CautionId | e.g. PeriphFailed_Lora, SettingsReset, SettingsStorageError, SafeMode |
| `text` | short string ≤24 chars | e.g. "LORA FAIL" |
| `active` | bool | |

Shown as an amber banner on `lv_layer_top()` (all pages), cycling through the active cautions every
3 s, and listed on the Settings status section. S4 will replace this with the AlertManager. Its
IDs are kept.

## 5. RailLease

| Field | Type | Notes |
| :--- | :--- | :--- |
| `rail` | enum Rail {Display, Lora, AudioAmp, Gnss, Nfc, Sensor} | |
| refcount | per-rail uint8 in PowerService | Rail on when >0, off when it reaches 0 |

RAII: constructing acquires, destroying releases. Not copyable, movable. In S0, after boot all
optional rails are forced off (refcount 0) except Display and Sensor (needed by touch/IMU).

## 6. Boot record (RTC_NOINIT)

| Field | Type | Notes |
| :--- | :--- | :--- |
| `magic` | u32 | Validity marker |
| `fastFailCount` | u8 | Incremented at boot, cleared after 10 s of stable uptime |
| `safeMode` | bool | Set when fastFailCount ≥ 3 |
| `lastStage` | enum BootStage {Start, PmuProbe, VendorInit, DisplayInit, UiInit, Stable} | Breadcrumb written **before** each boot stage starts. After a reset it tells which stage crashed |

**Boot mode decision** (pure, `core::BootGuard`):
```
fastFailCount < 3                          → Normal
fastFailCount ≥ 3 && lastStage == DisplayInit → Headless  (skip display/LVGL init; serial-only FATAL idle loop)
fastFailCount ≥ 3 otherwise                → Safe      (only PMU, display, touch; all optional NO_HW_* flags set)
```
In Headless mode the serial console still runs (`ad info`, `ad reboot`, `ad set reset`), so the
device can be diagnosed. It never resets on its own, so it can't loop.

## 7. Diagnostic snapshot (overlay)

`fps` (u16), `heapInternalFree` (u32), `heapInternalMin` (u32), `psramFree` (u32), updated at 1 Hz.

## 8. Event (EventBus)

`struct Event { uint16_t type; uint16_t pad; uint32_t a; uint32_t b; }` (12 bytes).
S0 types: `PageChanged(a=from,b=to)`, `SettingsChanged(a=keyHash)`, `CautionChanged`,
`ButtonClick`, `PowerSnapshotUpdated`.
