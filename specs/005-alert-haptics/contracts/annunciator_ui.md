# Contract: Annunciator Band, Haptic Patterns, Settings (S4)

## Annunciator band (`ui/AnnunciatorBar`, replaces the S0 `CautionBanner`)
- **Placement:** `lv_layer_top()`, top, full width, height **S(28)**, above every page. This is a drop-in for the S0
  banner: `PfdLayout` `bannerH` and the WX/Settings page top padding already reserve S(28).
- **Hidden** when no alert is active.

| State | Background | Text | Behaviour |
|---|---|---|---|
| Master is a WARNING | red (`Theme::warning()`) | white, `STALL  +2` | flashes at 1 Hz (100 % / 35 % opacity) while any alert is unacknowledged |
| Master is a CAUTION | amber (`Theme::caution()`) | black | same |
| Master is an ADVISORY | cyan (`Theme::advisory()`) | black | same |
| All acknowledged | the level colour of the shown alert | `CLOCK NOT SET  2/3` | steady; cycles through all active alerts every 3 s, highest first |

- **Tap** → acknowledge all + one click. The tap is consumed and never becomes a swipe or a control press below.
- **Text:** limited to the safe width at the band's vertical centre (rounded corners, r = 64). Long text ends with
  "…" (`LV_LABEL_LONG_DOT`).

## Haptic patterns (research R2; effect ids tuned on the device)

| Pattern | Used by | Intensity Off | Low | High | Quiet mode |
|---|---|---|---|---|---|
| Click | page change, control activation, band tap | none | Sharp Click 60 % | Sharp Click 100 % | unchanged |
| Advisory | ADVISORY raised | none | Soft Bump 60 % | Soft Bump 100 % | none |
| Caution | CAUTION raised | none | Double Click 60 % | Triple Click 100 % | none |
| Stick shaker | WARNING raised / every 2 s until ack | **full** | **full** | **full** | **full** |

**Priority:** stick shaker > caution > advisory > click. A lower priority never interrupts a higher one that is
still running. Clicks are dropped, not queued.

## Settings page: "ALERTS" section
- **Vibration:** dropdown Off / Low / High (`hapticlvl`).
- **Quiet mode:** switch (`quiet`).
- **Test alert** button: raises `Test1` (CAUTION, "TEST ALERT") and clears it after 10 s, so the wearer can feel the
  CAUTION pattern at the chosen strength.
