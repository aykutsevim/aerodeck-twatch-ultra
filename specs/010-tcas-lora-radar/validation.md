# Validation: TCAS LoRa Radar (Slice S9)

**Device**: T-Watch Ultra, 915 MHz hardware variant, COM3. **Region used on the device: EU868 only**
(869.525 MHz, 10 dBm). One device only: the two-device checks are pending (end of file).

## T001 — Baseline (branch `010-tcas-lora-radar`, before S9 code)

| Item | Value |
| :-- | :-- |
| `pio test -e native` | 269/269 |
| Ultra build | RAM 67 596 B (20.6 %), flash 2 064 701 B (49.2 %) |
| S3 build | SUCCESS (RAM 64 484 B, flash 1 900 465 B) |
| Screen-off current (S8) | 34 mA |

## T004 — Radio probe (Q0), 2026-11-16

| Step | Result |
| :-- | :-- |
| `ad periph` | `LORA present=1 state=OK` |
| `ad probe s9 tx` (EU868 869.525 MHz, SF9/BW125/CR4/5, 10 dBm, sync 0x12) | 3 × `done`, `err=0`; measured airtime **192 / 193 / 193 ms**; RadioLib `getTimeOnAir` **185 ms**; `core::LoraAirtime` **185 ms** (measured includes ~7 ms SPI/mode-switch overhead, +4 %, within ±10 %) |
| `ad probe s9 rx 20` / `rx 60` (sync 0x34, listen only) | the receiver runs and the probe completes; **0 packets, 0 CRC errors** (no LoRa traffic heard on 869.525 MHz at the wearer's location; noted, not a failure) |
| Rail | `rail lora ON` at probe start, `rail lora OFF` / `rails=lora:0` after |
| SPI sharing | no SD card is inserted (`SD ABSENT`), so the SD-during-probe check could not run; NFC is idle. Radio calls take LilyGoLib's SPI lock |

The 915 MHz hardware transmits normally at 869.525 MHz (transmit-done on every packet). R2 holds; no revision needed.

## T019 — Beaconing (Q1, SC-008)

TCAS on (EU868 869.525 MHz, SF9, 10 dBm, interval 30 s), serial log for 10 minutes:

| Item | Result |
| :-- | :-- |
| Beacons | device counter `sent` 3 → 17 over 423 s of log (**30.2 s average**); 21 sent in total by the end; `txfail=0`, `delayed=0`, `skipped=0` |
| Transmit-done | every logged beacon `tx done`, airtime **193 ms** each |
| Intervals (consecutive logged pairs) | 27.5 – 32.6 s (±10 % jitter). Two longer gaps (61.5 s, 118.7 s) are USB-CDC dropped log lines, not missing beacons: the device counter has no gap |
| Content | `beacon #n AD-2F90 batt=100`: version, id, sequence, callsign, battery only (18 B, no position) |
| Duty | 21 × 185 ms = **3.9 s / 36 s** |
| Persistence | after a reflash (chip reset) `ad tcas` showed `duty=4.4s`: the rolling-hour history was restored from RTC memory (research R4) |

### Q2 — Duty budget soak (SC-003, NFR-REG-001)

Test interval 5 s (`ad tcas interval 5`, demand ≈ 3.7 % of the hour, far above the 1 % budget), 62 minutes, `ad tcas`
polled every 60 s (63 polls):

| Item | Result |
| :-- | :-- |
| Device rolling-hour airtime | **max 35.9 s / 36 s** over 63 polls; it never exceeded the budget |
| Counters at the end | `sent=220 delayed=273 skipped=257 txfail=0`: the scheduler delayed, then skipped beacons once the budget was spent |
| Log cross-check | 132 `tx done` lines seen (USB CDC drops lines, so this is a lower bound); rolling-hour total from the logged beacons ≥ 22.2 s |
| Gate behaviour | `beacon delayed by duty (n s)` then `beacon skipped (duty)`; the manual `ad tcas beacon` answers `ERR duty (next in …)` |

Interval restored to the setting (30 s) at the end.

### Q7 — Region limits

| Step | Result |
| :-- | :-- |
| `ad set lorapwr 20` | accepted and stored; the radio restarts at **14 dBm** (EU868 limit): `ad tcas` shows `pwr=14` |
| `ad set lorasf 12` | the radio restarts at SF12; the queued beacon shows `air 1319 ms` (computed) and the duty gate uses it; measured 1321 ms |
| Restore | `lorasf 9`, `lorapwr 10`: the radio restarts at `SF9 BW125 10dBm` |
| `ad tcas off` | `rail lora OFF`, `on=0 radio=off` |

**Defect found and fixed:** at SF12 the first transmit logged `[att] no gravity samples for >1 s` / `status FAIL`.
`radio.transmit()` busy-polls DIO1 for the whole airtime while holding the SPI lock, so the `radio` task (priority 4)
starved the `svc` task (priority 2, IMU sampling) on core 0 for 1.3 s. `BoardUltra::radioSend` now calls
`startTransmit`, releases the SPI lock and blocks on the DIO1 semaphore (timeout 1.5 × airtime + 200 ms), then
`finishTransmit`. Re-test after the reflash: three SF12 beacons (`tx done air 1321 ms`) with no attitude fault.

## T023 — Radar with simulated contacts (Q3 radar part, Q6)

| Check | Result |
| :-- | :-- |
| Rings | `sim 11111111 ALPHA -45` → ring 0 (inner, amber), `BRAVO -70` → ring 2, `CHARLIE -95` → ring 3; bearings stable per id (241°, 219°, 291°) |
| Appearance | the bogeys appear on the next page refresh (< 1 s after injection) |
| EMA | a contact fed −80 after −40 moves out over updates (ema −52 → −60 → −70 → −75, ring 0 → 1 → 2) and back in on −40 |
| Age-out | default `tcasage` 300 s: the wearer saw the contacts gone after ~20 min, with the sweep still running (the expected age-out). `[tcas] lost …` logged |
| PFD fps, TCAS on | `ad att sweep on`, PFD: **fps 33 / 34 / 35** (≥ 30) |
| Radar page | 18 fps (sweep timer 20 Hz); heap **72 KB** free (≥ 40 KB), PSRAM 8.3 MB |
| Wearer (day) | "All good": the sweep rotates smoothly; 3 diamonds with callsign labels at different bearings; ALPHA amber in the inner ring |
| Wearer (NVG) | "Readable": all green; rings, sweep and labels readable; ALPHA distinguishable |
| Long run | 30 min on the TCAS page, screen kept on, 4 contacts, beaconing: no reset (uptime 222 → 1970 s) |

## T025 — Detail card (Q3 card part)

Wearer: **"Card OK, easy tap"**. Callsign, `#id` suffix, RSSI, SNR, "heard N s ago" counting up, BATT 50 %, SIM.
A tap closes the card. Bogeys are hit on the first try.

## T027 — TRAFFIC (Q4, SC-005)

| Check | Result |
| :-- | :-- |
| One per entry | contact from −95 to −40: **one** TRAFFIC at the update where ema crosses −55 (ring 0); further −40 updates: no repeat. Out to −80 (`alert clear id=44` once no contact is inner) and back in: one more. Clean in/out cycles: DELTA ×2, ECHO, FOXTROT, each exactly **one** TRAFFIC (**3 of 3** and more) |
| Voice | `aud play traffic prio=3`; wearer: **"Heard it"**, a clear "Traffic" once |
| Screen-off wake | the screen was off: `scr on (alert)` at the TRAFFIC; wearer saw the watch wake |
| Quiet mode | `ad set quiet 1`: TRAFFIC raised and the screen woke, **no** `aud play` (silent) |
| Acknowledge | `ad alert ack` → `ADVISORY TRAFFIC id=44 acked=1`; it clears when no contact is inner, like other advisories |

## T028 — Power and eject (Q5, SC-004, SC-007)

USB meter at 5 V, battery full, screen off (wearer readings):

| State | Meter |
| :-- | :-- |
| TCAS off (`rail lora OFF`) | **34 mA**, the same as S8 (SC-004) |
| TCAS on, between beacons | **40 mA** (+6 mA, ≈ +8 mA from the battery) |
| During a beacon | 54 mA peak, momentary |

- **SC-007:** `docs/power_report.md` § S9. TCAS on while worn with the overnight eject: ≈ 36 mA, ≈ 30 h (≤ 50 mA met).
  TCAS on 24 h without eject: ≈ 53 mA, which misses the target. TCAS is off by default.
- **Eject with TCAS on** (`ad eject`): the radio stops before sleep; the wearer saw the normal sleep current and woke
  the watch with the button. After the wake, `on=1`, beacons continue; **duty `22.6s` before and after** (the RTC
  history survives the eject).

## T033 — Final checks

| Item | Value |
| :-- | :-- |
| `pio test -e native` | **305/305** (baseline 269; +36 tests) |
| Ultra build | RAM 74 304 B (+6 708 B), flash 2 083 649 B (+18 948 B) |
| S3 build | SUCCESS, RAM 70 356 B (+5 872 B), flash 1 917 437 B (+16 972 B); no radio on the S3 board (stubs) |

## Success criteria

| SC | Status | Evidence |
| :-- | :-- | :-- |
| SC-001 | **Pending manual (second device)** | single device: a simulated contact appears < 1 s after injection |
| SC-002 | **Pending manual (second device)** | single device: injected signal strengths map to the expected rings; EMA moves contacts in and out |
| SC-003 | Met | 1-hour soak at 5 s: max 35.9 s / 36 s; delayed 273, skipped 257 |
| SC-004 | Met | `rail lora OFF` with TCAS off; screen-off current 34 mA, the same as S8 |
| SC-005 | Met with simulated contacts; **real device pending** | one TRAFFIC per entry, ≥ 3 of 3; voice, wake, quiet, acknowledge |
| SC-006 | Met | PFD 33–35 fps with TCAS on; wearer: sweep smooth |
| SC-007 | Met | +6 mA (5 V); power report § S9 |
| SC-008 | Met (TX); RX listening only | every beacon `tx done` at 869.525 MHz, 30.2 s average over 10 min; RX probe runs, 0 packets heard (no other LoRa nearby) |

## Open issues

1. **One panic during the wearer checks (not reproduced).** While the wearer used the TCAS page (tapping bogeys), the
   watch reset with `reset_reason=panic`. Core dump (flash 0xFF0000): task `loopTask` (UI), EXCCAUSE 28
   (LoadProhibited) at address 0, in the IDF SPI-master interrupt for the **display bus** (SPI3): `spi_intr` →
   `spi_bus_lock_bg_exit` → `resume_dev_in_isr`, while LVGL was invalidating an area. The radio is on the other bus
   (Arduino `SPI`, SPI2) and was not in a transfer. Not reproduced in four `simclear` repeats or in 30 minutes on the
   TCAS page with the screen on and contacts present. Kept open: watch for a recurrence; if it returns, record what
   was touched and re-read the dump.
2. **Range on 915 MHz hardware** (ADR-015): not measurable without a second device.

## Pending manual checks (need a second AeroDeck watch or a LoRa node)

- SC-001: two devices see each other within one interval (5/5).
- SC-002: 2 m → 100 m moves the bogey outward by at least one ring, and back.
- SC-005 with a real device: TRAFFIC on approach.
- RX decode of a real beacon (SC-008 receive part beyond "listening").
