# Contract: Serial Command Additions (S9)

| Command | Effect | Reply |
| :--- | :--- | :--- |
| `ad tcas` | State | `OK on=1 radio=ok region=EU868 freq=869.525 pwr=10 sf=9 bw=125 call=AD-1A2B id=1A2B3C4D int=30 sent=12 delayed=0 skipped=0 rx=0 foreign=0 crc=0 duty=2.2s/36s contacts=1` + one line per contact: `1A2B3C4D AD-9F00 rssi=-71(ema -70.2) snr=7.5 ring=2 brg=213 age=4s batt=88 sim=0` |
| `ad tcas on` · `ad tcas off` | Same as the `tcas` setting | `OK` |
| `ad tcas beacon` | **Test hook**: send one beacon now (duty gate applies) | `OK sent air=185ms` / `ERR duty (next in 120 s)` |
| `ad tcas sim <id-hex> <callsign> <rssi>` | **Test hook**: inject a contact as if received (`sim=1`) | `OK ring=…` |
| `ad tcas simclear` | **Test hook**: remove simulated contacts | `OK` |
| `ad tcas interval <s>` | **Test hook**: a temporary interval of 5..300 s (the duty gate still applies) | `OK` |
| `ad probe s9 tx` | **Test hook**: 3 beacons; prints transmit-done, measured vs computed airtime | `OK …` |
| `ad probe s9 rx <s>` | **Test hook**: listen-only with the public sync word 0x34 for s seconds; counts headers, packets and CRC errors | `OK …` |
| `ad set callsign <txt>` · `ad set tcasint <s>` · `ad set tcasage <s>` · `ad set lorareg <0/1>` · `ad set lorapwr <dBm>` · `ad set lorasf <7..12>` · `ad set lorabw <0..2>` | Settings (clamped by the region) | `OK` / `ERR …` |

**Logging:**
- `[tcas] on|off (rail lora …)`, `[tcas] radio start <region> <freq> SF<sf> BW<bw> <pwr>dBm`;
- `[tcas] beacon #<n> air <ms> duty <used>/<budget>`, `[tcas] beacon delayed by duty (<s> s)`;
- `[tcas] rx <id> <call> rssi=… snr=…`, `[tcas] rx foreign len=… rssi=…`;
- `[tcas] new contact …`, `[tcas] lost <call>`, `[tcas] TRAFFIC <call>`.

The callsign is user data chosen for broadcast; nothing else personal is logged.
