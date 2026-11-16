# Quickstart / Validation: TCAS LoRa Radar (Slice S9)

**One device only** (wearer, 2026-11-16). The two-device checks are marked *pending* and listed at the end.

## Prerequisites

- T-Watch Ultra (915 MHz hardware) on COM3; firmware flashed; `pio test -e native` green.
- Region **EU868** (default): 869.525 MHz, 10 dBm. **Do not select US915 in Türkiye.**
- Never run `ad set reset` without asking.

## Q0 — Radio probe (plan note 1)

1. `ad tcas on` → `[tcas] radio start EU868 869.525 SF9 BW125 10dBm`; `ad pwr log on` shows `lora:1`.
2. `ad probe s9 tx`: 3 × transmit-done; the measured airtime ≈ the computed ≈ 185 ms (±10 %).
3. `ad probe s9 rx 60`: the receiver runs; the header, packet and CRC counters are reported. Zero is possible if no
   LoRaWAN traffic is in range; that is noted, not a failure.
4. `ad tcas off` → `lora:0` in the profile line (rail off). An SD/NFC access during TCAS shows no SPI conflict.

## Q1 — Beaconing (US3, SC-008)

TCAS on, interval 30 s, 10 minutes: `sent` ≈ 20 (±1), every beacon with transmit-done, intervals jittered
27–33 s. The beacon contains only version, id, callsign, seq and battery (decode in the log).

## Q2 — Duty-cycle soak (US3, SC-003)

`ad tcas interval 5` for **1 hour** (720 attempts × 0.185 s ≈ 133 s wanted vs 36 s allowed):
- `duty` never exceeds 36 s in any rolling hour (`ad tcas` every minute, by script);
- `delayed`/`skipped` grow;
- no transmit happens while over budget.

## Q3 — Radar with simulated contacts (US1, US2, SC-001/002 single-device part)

- `ad tcas sim 11111111 ALPHA -45` → ring 0 (inner); `… BRAVO -70` → ring 2; `… CHARLIE -95` → ring 3.
- The bogeys appear within 1 s at stable bearings; the sweep rotates.
- Tap a bogey → the card shows callsign, RSSI, SNR, age (counting) and battery.
- Re-inject ALPHA at −90: it moves outward (EMA: a few updates).
- Wait for the age-out (`ad set tcasage 60`): the bogeys disappear after 60 s; the card shows LOST.

## Q4 — TRAFFIC (US4, SC-005)

- Inject a contact at −95, then three updates at −40: exactly one TRAFFIC advisory with the "Traffic" voice.
- Keep it at −40: no repeat.
- Move it out to −80, then back to −40: one more TRAFFIC.
- Repeat 3×. Quiet mode silences the voice.
- With the screen off, a TRAFFIC turns it on.
- Acknowledge a TRAFFIC: it clears like other advisories.

## Q5 — Power and eject (US3, SC-004, SC-007, FR-014)

- USB meter, screen off: TCAS off (expect the S8 value, 34 mA) vs TCAS on. Record the delta; the total with TCAS on
  must stay ≤ 50 mA (NFR-PWR-002). Update `docs/power_report.md`.
- Duty history: send a few beacons, eject, wake: `ad tcas` `duty` still shows the used time.
- Eject with TCAS on: the radio is off in sleep; after the wake TCAS resumes (`ad tcas` on=1, beacons continue).

## Q6 — Smoothness and page (FR-013, SC-006)

- PFD fps median ≥ 30 with TCAS on.
- On the radar page the sweep looks continuous (wearer).
- NVG: the radar is green and readable.

## Q7 — Region limits (FR-002)

- `ad set lorapwr 20` in EU868 → clamped to 14.
- `lorasf 12` is accepted, and its airtime is used by the duty gate.
- The page and the console show the region and frequency.

## Pending manual checks (need a second AeroDeck watch or LoRa node)

- SC-001: two devices see each other within one interval (5/5).
- SC-002: 2 m → 100 m moves the bogey outward by at least one ring.
- SC-005 with a real device: TRAFFIC on approach.
