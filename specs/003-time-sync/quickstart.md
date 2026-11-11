# Quickstart & Validation: Time Sync & Connectivity (S2)

Prerequisites: as in S1 (PlatformIO via `python -m platformio`, WinLibs GCC on PATH, watch on COM3, serial opened with
DTR/RTS low). A 2.4 GHz WPA2 network with internet access, and a phone.

## Q1. Build & tests
```powershell
pio test -e native            # S0/S1 suites + ntp, posix_tz, zones, sync_schedule, command_parser (quotes), settings v3
pio run -e twatch_ultra -t upload --upload-port COM3
pio run -e twatchs3
```

## Q2. Migration (FR-015)
Flash over S1 settings with `tzoff=180`: `ad set list` shows `tzid=<Istanbul>`, no `tzoff`; `ad tz` → `tz=Istanbul offset_min=180`.
LOCAL on the PFD is unchanged.

## Q3. USB provisioning + sync (US1, US2, SC-001, SC-003)
```text
ad wifi set "MyNet" "secret-pass"    → OK testing … [net] test: Ok
ad set list                           → wifipsk=***
ad time set 2020-01-01T00:00:00Z      → (clock deliberately wrong; caution CLOCK NOT SET)
ad reboot                             → within 30 s: [net] sync: Ok, CLOCK NOT SET cleared, `ad time` matches the PC UTC ±1 s
ad wifi                               → wifi=OFF between syncs
```
Repeat the reboot ×10 (SC-001). Wrong password: `ad wifi set "MyNet" "wrongpass1"` → `test: WrongPassword` ≤20 s.

## Q4. Phone setup hotspot (US1 scenarios 6–10, SC-007, SC-008)
Run on an **iOS and an Android** phone (FR-004c). Time one first-time run end to end (SC-007, ≤3 min).
Settings → "Set up Wi-Fi" → the watch shows a QR code, AeroDeck-XXXX, the code, a 5:00 countdown. Scan the QR with the phone camera, the
page opens, pick the network, enter the password, Save → the watch shows "Connected" and the time syncs. Check: the code is never in
the serial log; cancelling or 5-min expiry closes the AP (phone loses the network; `ad wifi` → wifi=OFF).

## Q5. Robustness (SC-004)
Configure an absent SSID → 10× `ad sync`: each `test/sync: NotFound|Timeout` ≤15 s. During the attempts, `ad att sweep on`
+ overlay FPS ≥28 (S1 median 30 minus ≤2), `ad stat` heapmin ≥40 KB.

## Q6. Time zones (US3, SC-006)
`ad tz set London` + `ad time set 2027-03-28T00:59:50Z` → LOCAL 00:59:5x → 02:00:0x. `ad time set 2027-10-31T00:59:50Z` →
01:59:5x → 01:00:0x. `ad tz set Kolkata` → LOCAL = UTC+5:30. ZULU unaffected throughout.

## Q7. TIME UNSYNC (US4)
`ad wifi age 90000` (25 h) → TIME UNSYNC caution; `ad sync` OK → cleared. The Settings page shows "Last sync: N min ago".

## Q8. 24-hour soak (SC-002, SC-005)
Leave it on USB power with the network available for 24 h, logging serial to a file: 24/24 `sync: Ok`, `wifi OFF` after each,
grep the log for the PSK → 0 hits.
