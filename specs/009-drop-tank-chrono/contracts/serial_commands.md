# Contract: Serial Command Additions (S8)

| Command | Effect | Reply |
| :--- | :--- | :--- |
| `ad chrono` | Timer state | `OK mode=countdown state=running phase=countdown rem=00:42.350 interval=01:00 frac=0.71 cycles=0 late=0 end_utc=…` |
| `ad chrono start [<seconds>]` | Start: the set length, or the given seconds (**test hook**, 1..7200) | `OK` |
| `ad chrono pause` · `ad chrono resume` · `ad chrono reset` | As the buttons | `OK` / `ERR not running` / `ERR not paused` |
| `ad chrono pomo <workSec> <breakSec>` | **Test hook**: start Pomodoro with test lengths in seconds | `OK` |
| `ad set chrlen <1..120>` · `ad set chrmode <0/1>` · `ad set chrwork <1..120>` · `ad set chrbrk <5/10/15>` | Settings | `OK` / `ERR …` |
| `ad probe s8 alarm <seconds>` | **Test hook**: deep sleep with only the RTC-alarm wake at +s (T-probe) | `OK sleeping` |

**Logging:**
- `[chrono] start <mm:ss> mode=<…>`, `[chrono] pause rem=…`, `[chrono] resume`, `[chrono] reset`;
- `[chrono] expired at <utc ms> (err <ms> ms)`;
- `[chrono] work done (cycle n)` / `[chrono] break done`;
- `[chrono] restored <state> rem=… [late]`;
- `[chrono] sleep hook: wake_in=<s>|postpone`;
- `[pwr] eject postponed: timer expiring` (PowerService).
