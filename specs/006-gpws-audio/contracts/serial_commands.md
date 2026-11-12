# Contract: Serial Command Additions (S5)

| Command | Effect | Reply |
| :--- | :--- | :--- |
| `ad audio` | Audio state | `OK` + `amp=OFF playing=- queued=0 played=12 skipped=0 volume=3 chime=1 qh=22-7 quiet=0` |
| `ad audio play <clip>` | Queue a clip at Sample priority (clips: `master_caution`, `master_warning`, `stall`, `pull_up`, `bingo_fuel`, `minimums`, `traffic`, `chime`) | `OK queued` / `OK suppressed (quiet\|volume off\|quiet hours)` / `OK duplicate` / `ERR queue full` / `ERR unknown clip` |
| `ad audio stop` | Stop playback and clear the queue | `OK` |
| `ad audio list` | Check every clip file (exists, header valid, duration) | `OK` + one line per clip: `chime ok 0.71s 16000Hz` / `traffic MISSING` / `stall BAD (format)` |
| `ad audio chimecheck <HH:MM:SS>` | **Test hook**: evaluate the chime rule for that local time on today's local date, with the current settings and the real last-chimed hour (no sound). `already` appears only for an hour that actually chimed | `OK due=1 reason=-` / `OK due=0 reason=quiet-hours\|quiet\|volume\|disabled\|not-hh00\|already` |
| `ad audio chimenow` | **Test hook**: play the chime through the normal gates (as if hh:00) | `OK` / `OK suppressed (…)` |
| `ad set volume <0..4>` · `ad set chime <0\|1>` · `ad set qhstart <0..23>` · `ad set qhend <0..23>` | Settings | `OK` / `ERR out of range` |

Logging:
- `[aud] amp ON` and `[aud] amp OFF after <ms>`;
- `[aud] play <clip> prio=<n> ms=<duration>`, `[aud] stop (<reason>)`;
- `[aud] skip <clip>: missing|bad format` (once per clip per boot);
- `[aud] output unavailable` (speaker not OK or I2S start failed; once per boot);
- `[aud] chime hh=14`.
