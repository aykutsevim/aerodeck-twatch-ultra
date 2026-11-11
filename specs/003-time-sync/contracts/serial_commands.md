# Contract: Serial Command Additions (S2)

Extends the S0/S1 contracts (same transport and reply format). Tokens may be **double-quoted** to include spaces
(`\"` and `\\` escape inside quotes).

| Command | Effect | Reply |
| :--- | :--- | :--- |
| `ad wifi` | Network status | `OK` + `configured=1 ssid=MyNet psk=*** busy=Idle last=Ok last_sync_utc=… age_s=… next_s=… wifi=OFF` |
| `ad wifi set "<ssid>" "<psk>"` | Save credentials, then run a test connection | `OK testing` (the result follows as a log line `[net] test: Ok|WrongPassword|NotFound|NoTimeResponse|Timeout`) or `ERR ssid 1..32 chars` / `ERR psk 8..63 chars` |
| `ad wifi forget` | Erase credentials (Wi-Fi stays off) | `OK` |
| `ad wifi test` | Connect + one NTP query, without changing the clock | `OK testing` + result log line |
| `ad wifi scan` | Scan nearby networks (≤15 s) | `OK scanning`; results follow as log lines `[net] scan: N networks` and `[net] scan: <rssi> <ssid>`, strongest first (asynchronous, so the serial console never blocks the svc watchdog) |
| `ad wifi setup` | Start the phone setup hotspot (same as the Settings button) | `OK setup starting (code shown on the watch)`; the log then shows `[setup] hotspot AeroDeck-1A2B up` (**the access code is NOT printed**) |
| `ad wifi setup stop` | Cancel setup | `OK` |
| `ad sync` | Sync now | `OK syncing` + result log line `[net] sync: Ok offset_ms=… server=…` |
| `ad tz` | Current zone | `OK` + `tz=Istanbul posix=<+03>-3 offset_min=180 dst=0` |
| `ad tz list [filter]` | List zones (filter is a case-insensitive substring) | `OK` + `<index> <name>` lines |
| `ad tz set <name\|index>` | Select a zone | `OK` or `ERR no such zone` |
| `ad wifi age <seconds>` | **Test hook**: pretend the last successful sync was N seconds ago (for TIME UNSYNC testing; not persisted) | `OK` |
| `ad set syncmin <15..1440>` | Sync interval (existing settings command) | `OK` / `ERR out of range [15..1440]` |

Secret handling: `ad set list` prints `wifipsk=***`; no command prints the PSK or the setup access code.
`ad time set` (S1) remains as the offline fallback (FR-012).
