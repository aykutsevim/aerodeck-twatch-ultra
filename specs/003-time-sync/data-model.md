# Data Model: Time Sync & Connectivity (S2)

## 1. Settings, schema v3

| Key | Type | Default | Range | Secret | Editable | Notes |
| :--- | :--- | :--- | :--- | :---: | :---: | :--- |
| `wifissid` | Str | "" | ≤32 chars | no | via setup/serial | empty = not configured |
| `wifipsk` | Str | "" | 8–63 chars (WPA2/WPA3-Personal; "" only when not configured) | **yes** | via setup/serial | never displayed or logged |
| `tzid` | U16 | index of "Istanbul" | 0…zoneCount−1 | no | yes (roller) | replaces `tzoff` |
| `syncmin` | U16 | 60 | 15…1440 | no | yes | sync interval in minutes |
| `lastsync` | I32 | 0 | ≥0 | no | no (internal) | UTC seconds of the last successful sync |

Removed: `tzoff` (S1). **Migration 2→3**: `tzid = Zones::bestMatch(tzoff)`; delete `tzoff`; write the defaults of the new keys.

## 2. Zone (built-in table, `core::Zones`)

| Field | Type | Example |
| :--- | :--- | :--- |
| `name` | const char* | "Istanbul", "London", "Kolkata", "UTC+05:45" |
| `posix` | const char* | "<+03>-3", "GMT0BST,M3.5.0/1,M10.5.0", "<+0545>-5:45" |

Order: 45 named zones (alphabetical), then the fixed offsets from −12:00 to +14:00. `bestMatch(offsetMin)` returns the first named
zone **without DST** whose standard offset equals `offsetMin`, else the fixed entry for `offsetMin`.

## 3. PosixTz rule (`core::PosixTz`)

`stdName, stdOffsetMin, hasDst, dstName, dstOffsetMin (default std+60), startRule{month, week(1–5), weekday(0–6),
timeSec}, endRule{…}`. `offsetMinutes(utc)`: DST is in effect when start ≤ local-standard-time < end (handles the
southern hemisphere, where start > end).

## 4. NetStatus (`Snapshot<NetStatus>`, written by the `net` task)

| Field | Type | Notes |
| :--- | :--- | :--- |
| `configured` | bool | SSID set |
| `busy` | enum {Idle, Syncing, Testing, Scanning, Setup} | |
| `lastResult` | enum {None, Ok, WrongPassword, NotFound, NoTimeResponse, Timeout, Rejected, Cancelled} | maps to FR-003: connected, wrong password, network not found, no time response, timed out, time rejected; Cancelled = setup cancelled/expired |
| `lastSyncUtc` | int64 | 0 = never |
| `nextDueMs` | uint32 | millis() of the next scheduled sync |
| `setupSsid` | char[16] | "AeroDeck-1A2B" while in setup |
| `setupCode` | char[9] | one-time WPA2 code (shown on the watch only, never logged) |
| `setupRemainingS` | uint16 | 300 → 0 |
| `wifiOn` | bool | radio state (SC-003 evidence) |

## 5. Sync schedule (`core::SyncSchedule`, pure)

```
onBoot(configured)        → due now if configured
onSuccess(now)            → next = now + syncmin; failures = 0
onFailure(now)            → failures++; next = now + backoff[min(failures,3)-1] (2, 5, 15 min), capped at syncmin
onConfigured()            → due now
isUnsynced(nowUtc, lastSyncUtc, configured) → configured && (lastSyncUtc == 0 || nowUtc − lastSyncUtc > 86400)
```

## 6. Setup session state machine

```
Idle --startSetup--> Scanning --> ApUp(code, 300 s) --POST /save valid--> Testing --> Idle (result shown)
                                         |--cancel / 300 s timeout--> Idle (Wi-Fi OFF)
Any state --error--> Idle (Wi-Fi OFF, result = Cancelled/Timeout)
```

## 7. NTP packet (`core::Ntp`, pure)

Request 48 B: byte0 = 0x23 (LI 0, VN 4, Mode 3), transmit timestamp = local send time (NTP era-0 seconds + fraction).
Response check: `(b0 & 7) == 4`, stratum 1–15, LI ≠ 3, originate == our transmit, transmit ≠ 0.
offset = ((T2 − T1) + (T3 − T4)) / 2, result = T4 + offset → UTC seconds and milliseconds.

## 8. Power rail

`core::Rail::Wifi` (new). Switched only by `PowerService` via a hook registered by `NetService`. `RailLease(Rail::Wifi)` is held for each transaction or setup session (constitution III). `NetStatus.wifiOn` mirrors `PowerService::refcount(Rail::Wifi) > 0`.

## 9. Cautions / events

`CautionId::TimeUnsync` "TIME UNSYNC" (advisory). Events: `NetStatusChanged`, plus the existing `ClockChanged` (from
`TimeService::set`) and `SettingsChanged` (for `tzid`).
