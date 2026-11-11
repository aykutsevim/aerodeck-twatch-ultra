# S2 Validation: Time sync & connectivity

**Device:** T-Watch Ultra (COM3), firmware from branch `003-time-sync` · **Date:** 2026-11-11
**Network:** user's home Wi-Fi (SSID not recorded), configured via the phone setup hotspot.
Scripts used live in the session scratchpad: `validate_s2.py`, `boot_check.py`, `boot_sync_diag.py`, `hang_boot.py`,
`pc_ntp_offset.py`, `validate_s1.py`. Logs were checked with the SSID redacted. The PSK is never printed by the firmware.

## Summary

| Criterion | Result | Evidence |
|---|---|---|
| SC-001 boot sync ≤30 s, ±1 s | ✅ 10/10 | §Q3: synced 8.2–11.2 s after reset, −0.24…+0.39 s vs NTP |
| SC-002 24/24 hourly syncs | ⏳ pending | Q8 24-h soak not yet run (T038) |
| SC-003 Wi-Fi off between syncs | ✅ (short runs) | `rail wifi OFF` after every sync (13/13 boot + 3/3 manual). 24-h confirmation pending (T038) |
| SC-004 no network → give up ≤15 s | ⏳ pending | Needs an absent SSID, which replaces the user's credentials. Not done without consent (T026) |
| SC-005 PSK never shown | ✅ code + logs so far | Secrets grep (T037) clean; `ad wifi`/`ad set list` print `psk=***`. 24-h log grep pending (T038) |
| SC-006 DST changeover exact | ✅ 2/2 | §Q6 |
| SC-007 phone setup ≤3 min | ⏳ partial | User completed setup on their phone. iOS + Android and timing not yet recorded (T020) |
| SC-008 hotspot ≤5 min, off otherwise | ✅ | §Setup: an unattended session closed at 300 s, `wifi=OFF` |

## Phase 2 / regression (T037)
- `pio test -e native`: **115/115** (test_ntp, test_posix_tz, test_zones, test_sync_schedule, test_settings v3,
  test_command_parser quoted tokens, plus the S0/S1 suites).
- `pio run -e twatchs3`: SUCCESS (also after the I2C recovery fix below).
- Layering (ADR-012): `WiFi.h`/`WiFiUdp.h`/`WebServer.h`/`DNSServer.h` appear only in `src/services/`; no `lv_` in services.
- Secrets: no log or `r.line` call takes `wifipsk`, the PSK buffer or the setup code (NetService, SetupPortal, SettingsStore).
- S1 regression (`validate_s1.py att perf regress`): PFD median 29 fps (min 28), attitude OK, the IMU simfail →
  IMU FAIL caution, the nav ring and all peripherals OK, boot→home 3689 ms (≤4 s).
  The S0 script's HOME-ring expectations were superseded by S1, so the S1 regress section replaces them.

## Bug found and fixed: PMU missing after a soft reset
- **Symptom:** in the first two SC-001 runs, 4 of 10 boots showed no sync. Raw capture showed those boots stopping at
  `FATAL: PMU (AXP2101 @0x34) not found; staying idle`, so `NetService` never started. It reproduced whenever
  `ad reboot` came ~14 s apart. It did not reproduce when reboots were 45 s apart.
- **Cause:** the soft reset interrupts an I2C transfer on the shared bus. The slave keeps SDA low, so the single PMU
  probe at boot fails.
- **Fix:** new `src/board/I2cRecover.h`. `i2c::probeWithRecovery()` clocks SCL up to 9× until SDA is released, sends
  a STOP, and retries the probe up to 3×. Used by `BoardUltra` and `BoardS3`.
- **Verification:** 12/12 rapid reboots (14 s spacing) were normal (before: the first boot stalled). SC-001 run then
  passed 10/10.

## Q3: boot sync (SC-001)
The PC clock was measured against NTP before and after (`pc_ntp_offset.py`, three servers agree): **the PC runs
1.24 s ahead of NTP**. The watch's offset vs the PC is therefore corrected by +1.24 s to get its offset vs NTP.
The watch shows whole seconds, so each reading has ±0.5 s quantisation (3 readings averaged).

| Boot | Synced after reset (s) | vs PC (s) | vs NTP (s) | Wi-Fi after |
|---|---|---|---|---|
| 1 | 11.2 | −1.03 | +0.19 | OFF |
| 2 | 10.2 | −1.06 | +0.16 | OFF |
| 3 | 10.2 | −1.44 | −0.23 | OFF |
| 4 | 10.2 | −0.82 | +0.39 | OFF |
| 5 | 9.1 | −1.12 | +0.09 | OFF |
| 6 | 8.2 | −1.46 | −0.24 | OFF |
| 7 | 11.1 | −1.11 | +0.11 | OFF |
| 8 | 10.2 | −1.17 | +0.05 | OFF |
| 9 | 10.2 | −1.27 | −0.06 | OFF |
| 10 | 9.1 | −0.95 | +0.27 | OFF |

**10/10 pass.** The sync starts after the deferred probe (~7.1 s) and completes ~1.5–4.5 s later. Per-sync
`offset_ms` (watch RTC error corrected at that sync) ranged −572…+989 ms across reboots.

## Q5: performance during real syncs
- 3/3 `ad sync` Ok, `rail wifi OFF` after each.
- With `ad att sweep on`, FPS during syncs: min 18, **median 29**, avg 28.3. The minimum is a single sample at Wi-Fi
  association, which is brief and on core 0 work. Median and average meet ≥28.
- heapmin **81.9 KB** in normal operation, 66.9 KB with the setup portal running (≥40 KB, so plan note 5 was not needed).
- **Pending (SC-004):** absent SSID 10× ≤15 s. It needs the user's consent, because it replaces the stored credentials.

## Q6 / Q2: time zones (SC-006)
- Europe/London: spring changeover (last Sunday in March, 01:00 UTC) → offset 0 → +60 at the exact second; autumn
  (last Sunday in October, 01:00 UTC) → +60 → 0 at the exact second. **2/2.**
- Asia/Kolkata: +330 min, no DST. `ad tz set Nowhere/Unknown` → `ERR`, zone unchanged.
- ZULU unaffected by zone changes. LOCAL updates ≤1 s after a zone change (T029).
- Q2: S1 `tzoff=180` migrated to Europe/Istanbul (+180). LOCAL was unchanged across the upgrade.

## Setup hotspot
- `ad wifi scan` returns the network list asynchronously (log). No SSIDs are recorded here.
- Unattended session: AP `AeroDeck-XXXX` closed by itself at 300 s, no credentials were saved, `wifi=OFF` (SC-008).
- The user completed setup from a phone: `configured=1 last=Ok wifi=OFF`, next sync scheduled ≈60 min. The serial log
  contains neither the setup code nor the PSK.
- **Pending (T020):** test on both iOS and Android, and time a first-time setup (≤3 min).

## Q7: TIME UNSYNC
- `ad wifi age 90000` (25 h) → **TIME UNSYNC** caution raised; `ad wifi age -1` + `ad sync` → `sync: Ok`, caution cleared.

## Pending manual / long-running checks
- **T017:** credential-change edge cases (change during a running sync; wrong password → `WrongPassword` ≤20 s).
  These need the user, because they replace the stored credentials.
- **T020:** iOS + Android setup and timing (SC-007).
- **T026 (part):** absent SSID ×10 (SC-004).
- **T038:** 24-h soak (SC-002, SC-003, SC-005 log grep).
