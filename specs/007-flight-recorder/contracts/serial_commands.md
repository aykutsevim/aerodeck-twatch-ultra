# Contract: Serial Command Additions (S6)

| Command | Effect | Reply |
| :--- | :--- | :--- |
| `ad rec` | Recorder state | `OK` + `today=2026-11-12 steps=4210 nm=1.70 maxg=2.7@14:05 stride=75 steps_src=OK accel=OK g_now=1.00 range=16 saves=12 last_save=312s rejected=0` + one line per chart day: `d-1 2026-11-11 Tue steps=8120 nm=3.29 maxg=3.1@09:12` / `d-3 2026-11-09 Sun empty` |
| `ad rec save` | Save now | `OK saved` / `ERR nvs` |
| `ad rec sim steps <n>` | **Test hook**: add n steps to today (1..100000) | `OK` |
| `ad rec sim g <x.x>` | **Test hook**: inject a peak of x.x g at the current time | `OK` / `OK lower (max stays 2.7)` |
| `ad rec rollover` | **Test hook**: act as if the local date advanced by one day | `OK rolled` |
| `ad rec clear` | **Test hook**: erase today and the history (NVS `rec` too) | `OK` |
| `ad set stride <30..150>` | Stride in cm | `OK` / `ERR out of range` |

Logging:
- `[rec] start today=<date|undated> steps=<n> hist=<k>` (boot, after loading the blob, or `load failed (<why>)`);
- `[rec] motion steps=<OK|absent> accel=<OK|absent> scale=<f> range=<g>` (after the deferred probe, `onImuReady`);
- `[rec] rollover <old date> -> <new date> (empty=<n>)`, `[rec] dated <date>` (first valid clock);
- `[rec] max g <x.x> at <HH:MM>`;
- `[rec] saved (<reason: periodic|rollover|shutdown|cmd>) <ms> ms`;
- `[rec] step jump rejected raw=<r> last=<l>`.
