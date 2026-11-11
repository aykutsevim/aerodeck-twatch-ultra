# Contract: WX Page and Settings Station Picker (S3)

## Page ring
`PFD → WX → PAGE B → SETTINGS`. WX replaces `PlaceholderA` (`PageId::Weather`). The indicator dots and `ad nav` names
show `WX`.

## WX page regions (portrait 410×502; all sizes through `S(px)`; everything inside the r = 64 rounded safe area)

| Region | Content | States |
|---|---|---|
| Header | `LTAC`, `1250Z`, category pill | Pill: `VFR` green / `MVFR` blue / `IFR` red / `LIFR` magenta / `--` grey. With no report: `LTAC` `NO DATA` |
| Annunciator row | Amber boxed text: `STALE` and/or the error (`NO WIFI`, `NET FAIL`, `NO REPORT`, `SRV ERROR`); right side: `upd 13:02Z` (fetched time) or `REFRESHING…` | Hidden when fresh and Ok. When busy, `REFRESHING…` replaces the fetched time |
| Compass | North-up rose with N/E/S/W ticks; arrow from the wind direction toward the centre; speed in the centre | `CALM`, `VRB` (sector arc when given), `--` |
| Fields | Beside the compass: `WIND 270° 07 kt` (gusts `G25`), `VIS 10 km+`, `QNH 1020 hPa`, `TEMP 10°C, dew 4°C`. Full-width row: `CLOUDS Few 1,600 ft, broken 3,000 ft` (plain language; `Sky clear`, `No significant cloud`, `CAVOK: no cloud below 5,000 ft`, `Sky obscured, vertical visibility 200 ft`). *(Changed after the user's review: the coded `T/DP 10/4` and `FEW016 BKN030` were not readable.)* | Each value is `--` when missing |
| Raw | Raw METAR in a small monospace font, wrapped; the container scrolls vertically if needed | Empty with NO DATA |
| Refresh | Button `REFRESH`, pinned to the bottom: 51 % wide × S(40) high (15 % smaller than the first 60 % × S(48), at the user's request; still ≈209 × 68 px). The raw box is sized to end 4 units above it | Disabled look while busy. A tap while busy does nothing. With Wi-Fi not set up: annunciator `NO WIFI` and no fetch |

Update rules:
- Rebuild the values when `WeatherSnapshot.seq` changes.
- Re-evaluate STALE every 30 s while visible.
- No per-frame animation.

## Settings: "Weather station" row
- Row: `Weather station   LTAC  >`.
- Tap → modal: four rollers A–Z (initially the current letters), plus `OK` / `Cancel`.
- `OK` → `SettingsStore::setStr(WxStation, …)` → close the modal. The row updates, and the WX page shows the new
  station with `NO DATA`/`REFRESHING…`.
- `Cancel` → no change.
