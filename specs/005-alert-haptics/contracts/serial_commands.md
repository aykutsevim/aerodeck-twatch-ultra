# Contract: Serial Command Additions (S4)

Extends the S0–S3 contracts (same transport, quoting and replies).

| Command | Effect | Reply |
| :--- | :--- | :--- |
| `ad alert` | List active alerts, master first | `OK` + `n=3 unacked=1` then one line per alert: `WARNING STALL id=40 acked=0` |
| `ad alert raise <id> [W\|C\|A] ["text"]` | Raise an alert. `<id>` is a number or a name (`stall`, `pullup`, `test1..3`, `clocknotset`, …). Level and text default to the table; for `test1..3`, they default to Caution / "TEST n" | `OK` / `ERR unknown id` / `ERR alert set full` |
| `ad alert clear <id>` | Clear an alert | `OK` / `ERR not active` |
| `ad alert ack` | Acknowledge all (same as tapping the band) | `OK acked=N` |
| `ad haptic <click\|advisory\|caution\|shaker\|stop>` | **Test hook**: play a pattern with the current intensity/quiet rules (shaker always plays) | `OK played` / `OK suppressed (off\|quiet)` / `ERR haptics unavailable` |
| `ad fuel sim <pct\|off> [chg\|nochg]` | **Extended (S1 test hook)**: override the battery percent and, optionally, the charging state fed to the battery alarm and EICAS. The watch charges while it is on USB, so `nochg` is needed to test STALL. Cleared by `off` or a reboot | `OK` / `ERR expected 0..100\|off [chg\|nochg]` |
| `ad set hapticlvl <0..2>` · `ad set quiet <0\|1>` | Settings (generic command) | `OK` / `ERR out of range` |

Logging:
- `[alert] raise WARNING STALL id=40`, `[alert] clear STALL`, `[alert] ack 2`;
- `[hap] play shaker prio=4 len=1500`, `[hap] drop click (warning playing)`, at DEBUG level.
