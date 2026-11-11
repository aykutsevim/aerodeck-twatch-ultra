# Contract: Serial Command Additions (S3)

Extends the S0–S2 contracts. The transport, quoting and reply format are unchanged.

| Command | Effect | Reply |
| :--- | :--- | :--- |
| `ad wx` | Weather state | `OK` + one line: `stn=LTAC has=1 cache=0 busy=Idle last=Ok obs_utc=… age_s=… stale=0 fetched_utc=… cat=MVFR`, then `raw=<METAR line>` |
| `ad wx fields` | The decoded fields of the current report | `OK` + `wind=270/07 gust=-- var=-- vis_m=10000 vis="10 km+" cavok=0 ceil_ft=3000 clouds="FEW016 BKN030" t=10 dp=4 qnh=1020 cat=MVFR` |
| `ad wx fetch` | Fetch now (same as the refresh button; coalesced) | `OK fetching` or `OK already pending` or `ERR wifi not set up`; the result follows as a log line `[wx] fetch: Ok obs=1250Z cat=MVFR ms=…` / `[wx] fetch: NoReport|NetFail|ServerError (detail)` |
| `ad wx set <ICAO>` | Set the station (4 letters, case-insensitive; triggers a fetch) | `OK stn=LTFM` or `ERR station must be 4 letters A-Z` |
| `ad wx decode "<METAR text>" [show]` | **Test hook**: decode the text with the on-device decoder and print the fields as `ad wx fields` does. Without `show` it changes nothing. With `show`, it also loads the report into the snapshot (RAM only, `obsUtc` = now, not cached) so the WX page renders it | `OK` + fields line, or `ERR no station/time` |
| `ad wx clear` | Erase the cached report (snapshot back to NO DATA) | `OK` |
| `ad wx age <seconds>` | **Test hook**: pretend the observation is N s old (−1 = off; RAM only). Used for STALE testing | `OK` |
| `ad set wxstn <ICAO>` | Same as `ad wx set` through the generic settings command | `OK` / `ERR invalid` |

Logging:
- `[wx]` lines carry the station, the result, the duration, and a TLS error code on failure.
- The full raw METAR is logged at INFO once per new observation (it is public data).
- No credentials appear.
