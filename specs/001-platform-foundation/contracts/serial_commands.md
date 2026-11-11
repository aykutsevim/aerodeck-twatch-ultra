# Contract: USB Serial Diagnostic Commands (S0)

Transport: USB-CDC, 115200 baud, line-based ASCII, `\n` or `\r\n` terminated, max 128 chars per line.
Every command starts with `ad `. Unknown commands reply `ERR unknown command`.
Replies start with `OK` or `ERR <reason>`, optionally followed by data lines, and end with a blank line.
Commands are parsed on the `svc` task and never block the UI.

| Command | Effect | Reply |
| :--- | :--- | :--- |
| `ad help` | List commands | `OK` + one line per command |
| `ad info` | Firmware version, board, uptime, reset reason, safe-mode flag | `OK` + `key=value` lines |
| `ad periph` | Peripheral status table | `OK` + `<name> present=<0/1> state=<OK/FAILED/ABSENT/SIMFAIL>` lines |
| `ad set list` | All settings | `OK` + `<key>=<value>` lines (secret values shown as `***`) |
| `ad set get <key>` | One setting | `OK` + `<key>=<value>` or `ERR no such key` |
| `ad set <key> <value>` | Validate + persist + apply | `OK` or `ERR out of range [min..max]` / `ERR not editable` |
| `ad set reset` | Restore defaults (no confirmation on serial) | `OK` |
| `ad simfail <name\|none> ...` | Set simulated-failure mask (`lora nfc gnss sd mic haptics imu rtc`), applies on reboot | `OK mask=0x..` |
| `ad log <error\|warn\|info\|debug>` | Set log level (persisted) | `OK` |
| `ad reboot` | Restart | `OK` then restart |
| `ad stat` | One-shot diagnostic snapshot | `OK` + `fps=.. heap=.. heapmin=.. psram=..` |

Log line format (all output not part of a reply):
`[<uptime_ms>][<E|W|I|D>][<tag>] <message>`. Secret setting values never appear in any output.
