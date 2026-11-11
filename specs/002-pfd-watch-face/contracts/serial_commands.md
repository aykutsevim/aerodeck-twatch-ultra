# Contract: Serial Command Additions (S1)

Extends `specs/001-platform-foundation/contracts/serial_commands.md` (same transport and reply format).

| Command | Effect | Reply |
| :--- | :--- | :--- |
| `ad time` | Current clock | `OK` + `utc=2026-11-11T14:37:20Z valid=1 tzoff=180 local=17:37:20` |
| `ad time set <YYYY-MM-DDTHH:MM:SSZ>` | Set RTC + system clock (UTC), clear `CLOCK NOT SET` | `OK` or `ERR bad time (expected YYYY-MM-DDTHH:MM:SSZ)` / `ERR rtc write failed` |
| `ad att` | Attitude diagnostics | `OK` + `status=OK pitch=-1.2 roll=0.4 lock=1 rate_hz=100 lat_ms=18 lat_max_ms=31` |
| `ad fuel` | EICAS diagnostics | `OK` + `pct=82 chg=0 temp_c=31.5 flow_pph=4.2 low=0 samples=14` (`flow_pph=--` when unavailable) |
| `ad fuel sim <pct\|off>` | **Test hook**: override the battery percent fed to the LOW FUEL logic and EICAS (cleared by `off` or reboot) | `OK` or `ERR expected 0..100\|off` |
| `ad att sweep <on\|off>` | **Test hook**: drive the horizon with synthetic motion (roll ±30° @0.5 Hz, pitch ±10° @0.3 Hz) for repeatable FPS measurement (SC-001) | `OK` or `ERR expected on\|off` |
| `ad set tzoff <minutes>` | Existing settings command; step 30 enforced | `OK` / `ERR out of range [-720..840 step 30]` |

All S0 commands are unchanged. `ad stat` keeps reporting FPS.
