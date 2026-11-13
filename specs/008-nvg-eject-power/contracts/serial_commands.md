# Contract: Serial Command Additions (S7)

| Command | Effect | Reply |
| :--- | :--- | :--- |
| `ad screen` | Display power state | `OK on=1 cause=raise idle=4.2s timeout=15s raise=on(avail) nvg=0 bright=80%` |
| `ad screen off` / `ad screen on` | **Test hook**: force the screen off or on, as the policy would | `OK` |
| `ad screen raise` | **Test hook**: inject a raise event | `OK` / `OK ignored (on)` |
| `ad nvg on` / `ad nvg off` | Toggle NVG; same as the `nvg` setting (persisted) | `OK nvg=1` |
| `ad eject` | Start the eject sequence; same as the gesture (`EjectRequested`, source 1) | `OK ejecting` (the last line before sleep), or `ERR unsupported` |
| `ad wake` | Cause of the last wake, and the eject timing recorded before sleep | `OK wake=button eject_ms=412 sleep_at=…` / `OK wake=poweron` |
| `ad pwr log on` / `ad pwr log off` | 1 Hz power profile (not persisted) | `OK` |
| `ad set nvg <0/1>` | Setting | `OK` |
| `ad set scrto <0..5>` | Timeout index: 10 s, 15 s, 30 s, 1 min, 5 min, Never | `OK` / `ERR out of range` |
| `ad set raise <0/1>` | Setting | `OK` |
| `ad set attref <0..90>` | Horizon tilt in degrees, 15° steps (FR-023) | `OK` / `ERR out of range` |
| `ad boot` | Boot (or wake) to first watch-face frame | `OK boot_to_home_ms=3428 wake=button` |
| `ad screen trace on\|off` | **Test hook**: log normalised gravity at 20 Hz (`[grav] ms x y z`), for tuning raise-to-wake | `OK` |
| `ad probe s7 sleep\|nvg\|day\|wake\|i2c` | **Test hooks** (T006): raw deep sleep with button wake · NVG filter on/off without the setting · wake cause · I2C bus scan at 100 kHz | `OK` + result |

**Profile line** (1 Hz while on):

```text
[pwr] prof t=1234 pct=78 mv=3987 chg=0 vbus=0 flow=4.1 scr=off nvg=0 rails=lora:0,amp:0,gnss:0,nfc:0,wifi:0 ma=n/a
```

`flow` is the S1 FuelFlow estimate in %/h, or `n/a` while it warms up.

**Logging:**
- `[scr] off (idle <n> s)`;
- `[scr] on (<button|raise|alert|touch|cmd>) <ms> ms` (time from the wake decision to the first flushed frame);
- `[nvg] on` / `[nvg] off`;
- `[eject] armed`, `[eject] disarmed (<timeout|short|side|tap>)`, `[eject] fire`;
- `[pwr] eject: rec saved <ms> ms; audio/haptic/net stopped; hooks=<n> wake_in=<s|none>; ui quiesced <ms> ms;
  sleeping at +<ms> ms`. The last line is flushed before sleep; the timing is also kept in RTC memory for
  `ad wake`.
- `[boot] wake=<poweron|button|imu|timer|other>`.

**Deliberately excluded:** no console command bypasses the deliberate-gesture rule on the watch itself. `ad eject`
is a developer hook over USB (NFR-USE-003 concerns the touch UI).
